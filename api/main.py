from flask import Flask, request, jsonify, Response
from flask_sock import Sock
import ollama
from ollama_service import ask_ai
from redis_controller import RedisController
import threading
import shortuuid
from transcriptor import Transcriptor

app = Flask(__name__)
sock = Sock(app)

# constants
url = "127.0.0.1:11434/api/chat"
AUDIO_STREAM_EXPIRE_TIME = 120  # 5 min sec
TRANSCRIPTION_CHUNK_SIZE = 48_000 # 3 seconds of audio at 16kHz, 16-bit PCM
TRANSCRIPTION_WINDOW_SIZE = 0 # 1.5 seconds of audio at 16kHz, 16-bit PCM
TRANS_MODEL = "base"
CHAT_MODEL = "llama3.2"
PREFIX_AUDIO_STREAM = "audio_stream:" # recorded audio
PREFIX_AUDIO_TRANS = "audio_trans:" # transcription

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
    audio_stream_id = None
    while True:
        message = ws.receive()
        
        if message is None:
            break
        if isinstance(message, str):
            if message.startswith("AUDIO STREAM START"):
                just_id = shortuuid.uuid()
                audio_stream_id = PREFIX_AUDIO_STREAM + just_id
                redis_controller.rpush(audio_stream_id, "START")  # Push a value to indicate the end of the session
                # thread for data transcription
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
            if message.startswith("AUDIO STREAM END"):
                # Send a signal to the task to stop processing and clean up resources
                redis_controller.rpush(audio_stream_id, "END")  # Push a value to indicate the end of the session
                redis_controller.setTTL(audio_stream_id, AUDIO_STREAM_EXPIRE_TIME)
        if isinstance(message, bytes):
            redis_controller.rpush(audio_stream_id, message)
            redis_controller.setTTL(audio_stream_id, AUDIO_STREAM_EXPIRE_TIME)


def worker_transcribe(just_id):
    print(f"[WORKER] Started: {just_id}")
    remaining_audio = bytearray()
    segment_to_transcribe = b""
    new_segment_ready_for_transcription = False
    segment_counter = 0
    trans_key = PREFIX_AUDIO_TRANS + just_id
    
    while True:
        enough_audio_bytes_for_transcription = False
        new_segment_ready_for_transcription = False
        # Wait for a new audio chunk to be available in Redis
        audio_stream_id = PREFIX_AUDIO_STREAM + just_id
        _, element = redis_controller.blpop(audio_stream_id, AUDIO_STREAM_EXPIRE_TIME)
        isEnd = element == b"END"
        isStart = element == b"START"
        isAudioChunk = not isEnd and not isStart and element is not None
         
        if element is None:
            break  # Exit the loop if no more audio chunks are available
        
        if isAudioChunk:
            # print(f"[WORKER] Received audio chunk: {len(element)} bytes")
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
        elif isEnd:
            segment_to_transcribe = remaining_audio
            remaining_audio = bytearray()
            new_segment_ready_for_transcription = True
        
        if (new_segment_ready_for_transcription):
            segment_counter += 1
            for words in transcriptor.transcribe(segment_to_transcribe):
                print(f"[WORKER] Transcribed segment {segment_counter} {trans_key}: {words}")
                redis_controller.rpush(
                    trans_key, 
                    f"{segment_counter}:" + words
                )
                redis_controller.setTTL(trans_key, AUDIO_STREAM_EXPIRE_TIME)
                
        if isEnd:
            # segment_to_transcribe = processed_audio_bytes
            print(f"[WORKER] Received end signal for: {audio_stream_id}")
            redis_controller.rpush(trans_key, f"END")
            redis_controller.setTTL(trans_key, AUDIO_STREAM_EXPIRE_TIME)
            break  # Exit the loop if the end signal is received
        
        if isStart:
            print(f"[WORKER] Received start signal for: {audio_stream_id}")
            continue  # Skip processing if the start signal is received

def worker_ask(just_id, ws):
    trans_key = PREFIX_AUDIO_TRANS + just_id
    question = ""
    
    while True:
        # check for transcriptions
        _, element = redis_controller.blpop(trans_key)
        print(f"[WEBSOCKET] Transcription element: {element}")
        if element is None:
            continue
        if element == b"END":
            ws.send("/END")
            break
        else:
            id_seg, text = element.decode('utf-8').split(":", 1)
            question += f" {text}"
            ws.send(text)
            
    # for sentense in ask_ai(client, CHAT_MODEL, question):
        

if __name__ == '__main__':
    app.run(debug=True, host="0.0.0.0", port=5000)