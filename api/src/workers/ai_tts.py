

from src.config import ( PREFIX_AI_TTS, AI_TTS_START, AI_TTS_END, 
    TTL_EXPIRE_TIME, LANGUAGE_MAP, PREFIX_LANGUAGE )
from piper import PiperVoice
from src.services.audio_process_utils import resample_audio
from simple_websocket.errors import ConnectionClosed

async def worker_ai_tts(just_id, ws, redis_controller): # purpose: send ai text answer to the client
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

    finally:
        print("[AI TTS] Worker finished")