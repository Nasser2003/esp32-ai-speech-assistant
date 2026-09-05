from flask import Flask, request, jsonify, Response
from flask_sock import Sock
import ollama
from ollama_service import ask_ai
from redis_controller import RedisController
import threading
import shortuuid
from transcriptor import Transcriptor
from piper import PiperVoice
import io
import wave
from simple_websocket.errors import ConnectionClosed
from audio_process_utils import resample_audio
from test_utils import test_received_audio, test_audio_tts

app = Flask(__name__)
sock = Sock(app)

# constants
url = "127.0.0.1:11434/api/chat"
TTL_EXPIRE_TIME = 120  # 5 min sec
TRANSCRIPTION_CHUNK_SIZE = 64_000 # 2 seconds of audio at 16kHz, 16-bit PCM
TRANSCRIPTION_WINDOW_SIZE = 32_000 # clide every 1 second of audio
TRANS_MODEL = "small"
CHAT_MODEL = "llama3.2"

# constant strings
PREFIX_LANGUAGE = "audio_language:" # recorded audio
PREFIX_RECORD = "audio_record:" # recorded audio
PREFIX_TRANSCRIPTION = "audio_trans:" # transcription
PREFIX_AI_TEXT = "ai_text:" # ai text response
PREFIX_AI_TTS = "ai_tts:" # ai audio response

RECORDING_START = "/RECORDING START" # start recording audio stream
RECORDING_END = "/RECORDING END"
TRANSCRIPTION_START = "/TRANSCRIPTION START" # start transcription of the audio stream
TRANSCRIPTION_END = "/TRANSCRIPTION END"
AI_TEXT_START = "/AI TEXT START" # start sending ai textual answer to the client
AI_TEXT_END = "/AI TEXT END"
AI_TTS_START = "/AI TTS START" # start sending ai audio answer to the client
AI_TTS_END = "/AI TTS END"
WEBSOCKET_CLOSE = "/WEBSOCKET CLOSE"

VOICE_LANGUAGE = "auto" # "en" or "fr" or "auto"
LANGUAGE_MAP = {
    "en": ["data/en_US-ryan-low.onnx", 16_000],
    "fr": ["data/fr_FR-upmc-medium.onnx", 22_050],
    "ru": ["data/ru_RU-ruslan-medium.onnx", 22_050],
}

# variables
client = ollama.Client()
redis_controller = RedisController(host='localhost', port=6379, db=0)
transcriptor = Transcriptor(TRANS_MODEL)

@app.route('/ask', methods=['POST'])
def ask():
    data = request.get_json()
    question = data.get('question')
    
    # send prompt to the model
    return Response(
        ask_ai(client, CHAT_MODEL, question),
        mimetype="text/plain"
    )


@app.route('/', methods=['GET'])
def home():
    return "Welcome to the AI Question Answering API!"


@sock.route("/ws")
def websocket(ws):
    just_id = shortuuid.uuid()
    audio_stream_id = PREFIX_RECORD + just_id
    
    while True:
        message = ws.receive()
        IS_SIGNAL = isinstance(message, str)
        IS_RECORDING_START = IS_SIGNAL and message.startswith(RECORDING_START)
        IS_RECORDING_END = IS_SIGNAL and message.startswith(RECORDING_END)
        IS_WEBSOCKET_CLOSE = IS_SIGNAL and message.startswith(WEBSOCKET_CLOSE)
        
        if message is None:
            break
        if IS_RECORDING_START:
            redis_controller.r_push_expire(audio_stream_id, RECORDING_START, TTL_EXPIRE_TIME)  # Push a value to indicate the end of the session
            # thread for audio transcription
            threading.Thread(
                target=worker_transcribe,
                args=(just_id,),
                daemon=True
            ).start()
            # thread for asking ai
            threading.Thread(
                target=worker_ask,
                args=(just_id, ws,),
                daemon=True
            ).start()
            # thread for sending ai tts audio
            threading.Thread(
                target=worker_ai_tts,
                args=(just_id, ws,),
                daemon=True
            ).start()
            # thread for sending ai answer
            threading.Thread(
                target=worker_ai_answer,
                args=(just_id, ws,),
                daemon=True
            ).start()
        if IS_RECORDING_END:
            redis_controller.r_push_expire(audio_stream_id, RECORDING_END, TTL_EXPIRE_TIME)  # Push a value to indicate the end of the session
        if IS_WEBSOCKET_CLOSE:
            ws.close()
        if isinstance(message, bytes):
            redis_controller.r_push_expire(audio_stream_id, message, TTL_EXPIRE_TIME)


def worker_transcribe(just_id):
    remaining_audio = bytearray()
    segment_to_transcribe = b""
    new_segment_ready_for_transcription = False
    segment_counter = 0
    trans_key = PREFIX_TRANSCRIPTION + just_id
    lang_key = PREFIX_LANGUAGE + just_id
    print(f"[WORKER] Started: {trans_key}")
    
    temp_wav = b""
    
    while True:
        enough_audio_bytes_for_transcription = False
        new_segment_ready_for_transcription = False
        # Wait for a new audio chunk to be available in Redis
        audio_stream_id = PREFIX_RECORD + just_id
        _, element = redis_controller.blpop(audio_stream_id, TTL_EXPIRE_TIME)
        print(f"[WORKER] Received transcription element: {len(element)} bytes")
        IS_RECORDING_START = element == bytes(RECORDING_START, 'utf-8')
        IS_RECORDING_END = element == bytes(RECORDING_END, 'utf-8')
        isAudioChunk = not IS_RECORDING_END and not IS_RECORDING_START
         
        if element is None:
            continue
        elif isAudioChunk:
            # print(f"[WORKER] Received audio chunk: {len(element)} bytes")
            temp_wav += element
            remaining_audio.extend(element)
            is_first_segment = segment_counter == 0
            enough_audio_bytes_for_transcription = len(remaining_audio) >= TRANSCRIPTION_CHUNK_SIZE
            
        if enough_audio_bytes_for_transcription:
            if is_first_segment: # for the first segment, no need to glide the window
                segment_to_transcribe = bytes(remaining_audio[:TRANSCRIPTION_CHUNK_SIZE])
                del remaining_audio[:TRANSCRIPTION_CHUNK_SIZE]
                new_segment_ready_for_transcription = True
            else:
                SPLITTER = TRANSCRIPTION_CHUNK_SIZE-TRANSCRIPTION_WINDOW_SIZE
                first_part = segment_to_transcribe[SPLITTER:]
                second_part = bytes(remaining_audio[:SPLITTER])
                segment_to_transcribe = (first_part + second_part)
                del remaining_audio[:SPLITTER]
                segment_to_transcribe = first_part + second_part
                new_segment_ready_for_transcription = True
        elif IS_RECORDING_END:
            segment_to_transcribe = remaining_audio
            remaining_audio = bytearray()
            new_segment_ready_for_transcription = True
        
        if (new_segment_ready_for_transcription):
            segment_counter += 1
            lang = VOICE_LANGUAGE
            if lang == "auto":
                lang = None
            for words, lang in transcriptor.transcribe(segment_to_transcribe, lang):
                redis_controller.r_push_expire(
                    lang_key, 
                    lang, 
                    TTL_EXPIRE_TIME
                )
                redis_controller.r_push_expire(
                    trans_key, 
                    f"{segment_counter}:{words}",
                    TTL_EXPIRE_TIME
                )
                
        if IS_RECORDING_START:
            redis_controller.r_push_expire(trans_key, TRANSCRIPTION_START, TTL_EXPIRE_TIME)
            print(f"[WORKER] Received start signal for: {audio_stream_id}")
            continue  # Skip processing if the start signal is received
        
        if IS_RECORDING_END:
            # segment_to_transcribe = processed_audio_bytes
            print(f"[WORKER] Received end signal for: {audio_stream_id}")
            redis_controller.r_push_expire(trans_key, TRANSCRIPTION_END, TTL_EXPIRE_TIME)
            break  # Exit the loop if the end signal is received
    test_received_audio(temp_wav)
        

def worker_ask(just_id, ws): # purpose: process transcription depending on window size
    try:
        trans_key = PREFIX_TRANSCRIPTION + just_id
        ai_tts_key = PREFIX_AI_TTS + just_id
        ai_text_key = PREFIX_AI_TEXT + just_id
        question = ""
        
        while True:
            # check for transcriptions
            _, element = redis_controller.blpop(trans_key, TTL_EXPIRE_TIME)
            IS_TRANSCRIPTION_START = element == bytes(TRANSCRIPTION_START, "utf-8")
            IS_TRANSCRIPTION_END = element == bytes(TRANSCRIPTION_END, "utf-8")
            print(f"[WEBSOCKET] Transcription element: {element.decode('utf-8') if element else 'None'}")
            
            if element is None:
                continue
            elif IS_TRANSCRIPTION_START:
                ws.send(TRANSCRIPTION_START)
            elif IS_TRANSCRIPTION_END:
                ws.send(TRANSCRIPTION_END)
                break
            else:
                id_seg, text = element.decode('utf-8').split(":", 1)
                question += f"{text} "
                ws.send(text)
        
        
        redis_controller.r_push_expire(ai_tts_key, AI_TTS_START, TTL_EXPIRE_TIME)
        redis_controller.r_push_expire(ai_text_key, AI_TEXT_START, TTL_EXPIRE_TIME)
        
        for sentense in ask_ai(client, CHAT_MODEL, question):
            redis_controller.r_push_expire(ai_tts_key, sentense, TTL_EXPIRE_TIME)
            redis_controller.r_push_expire(ai_text_key, sentense, TTL_EXPIRE_TIME)
            ws.send(sentense)
            
        redis_controller.r_push_expire(ai_tts_key, AI_TTS_END, TTL_EXPIRE_TIME)
        redis_controller.r_push_expire(ai_text_key, AI_TEXT_END, TTL_EXPIRE_TIME)
    except ConnectionClosed as e:
        print(f"[AI ASK] WebSocket closed: {e}")


    # except Exception as e:
    #     print(f"[AI TTS] Unexpected error: {e}")

    finally:
        print("[AI ASK] Worker finished")
    
    
def worker_ai_tts(just_id, ws): # purpose: send ai text answer to the client
    try:
        ai_tts_key = PREFIX_AI_TTS + just_id
        
        while True:
            _, element = redis_controller.blpop(ai_tts_key, TTL_EXPIRE_TIME)
            IS_AI_TTS_START = element == bytes(AI_TTS_START, "utf-8")
            IS_AI_TTS_END = element == bytes(AI_TTS_END, "utf-8")
            
            if element is None:
                continue
            elif IS_AI_TTS_START:
                ws.send(AI_TTS_START)
            elif IS_AI_TTS_END:
                ws.send(AI_TTS_END)
                break
            else:
                # Determine the segment language for TTS synthesis
                lang = redis_controller.get_majoritary(PREFIX_LANGUAGE + just_id)
                redis_controller.setTTL(PREFIX_LANGUAGE + just_id, TTL_EXPIRE_TIME)  # Reset the TTL for the language key
                # print(f"[AI TTS] Detected language: {lang}\n")
                if lang is None:
                    lang = "en"
                if LANGUAGE_MAP.get(lang) is None:
                    print(f"Warning: Language not supported for TTS synthesis ({lang}). Defaulting to English.")
                    lang = "en"
                lang_path = LANGUAGE_MAP.get(lang)[0]
                sample_rate = LANGUAGE_MAP.get(lang)[1]
                config_path = lang_path + ".json"
                voice = PiperVoice.load(lang_path, config_path)
            
                ai_respons_segment = element.decode("utf-8")

                for chunk in voice.synthesize(ai_respons_segment):
                    audio_bytes = chunk.audio_int16_bytes
                    audio_bytes = resample_audio(audio_bytes, sample_rate, 16_000)
                    ws.send(audio_bytes)

    except ConnectionClosed as e:
        print(f"[AI TTS] WebSocket closed: {e}")

    # except Exception as e:
    #     print(f"[AI TTS] Unexpected error: {e}")

    finally:
        print("[AI TTS] Worker finished")
            
    
def worker_ai_answer(just_id, ws): # purpose: send ai tts audio answer to the client
    try:
        ai_text_key = PREFIX_AI_TEXT + just_id
        
        while True:
            _, element = redis_controller.blpop(ai_text_key, TTL_EXPIRE_TIME)
            IS_AI_TEXT_START = element == bytes(AI_TEXT_START, "utf-8")
            IS_AI_TEXT_END = element == bytes(AI_TEXT_END, "utf-8")
            if element is None:
                continue
            elif IS_AI_TEXT_START:
                ws.send(AI_TEXT_START)
            elif IS_AI_TEXT_END:
                ws.send(AI_TEXT_END)
                break
            else:
                print(f"[AI ANSWER] Sending AI text answer: {element.decode('utf-8')}")
                ws.send(element.decode('utf-8'))
    except ConnectionClosed as e:
        print(f"[AI ANSWER] WebSocket closed: {e}")

    # except Exception as e:
    #     print(f"[AI ANSWER] Unexpected error: {e}")

    finally:
        print("[AI ANSWER] Worker finished")


if __name__ == '__main__':
    # test_audio()
    app.run(debug=True, host="0.0.0.0", port=5000)