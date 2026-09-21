from fastapi import WebSocket
from piper import PiperVoice
from simple_websocket.errors import ConnectionClosed

from langdetect import detect
from services.audio_process_utils import resample_audio
from databases.redis_db import RedisDatabase
from config import ( REDIS_KEY_PREFIX_AI_TTS, SIGNAL_AI_TTS_START, SIGNAL_AI_TTS_END, 
    LANGUAGE_MAP, TTS_CHUNK_SIZE )

# purpose: send ai text answer to the client
async def worker_ai_tts(just_id, client_ws: WebSocket, redis_db: RedisDatabase): 
    try:
        ai_tts_key = REDIS_KEY_PREFIX_AI_TTS + just_id
        
        while True:
            result = await redis_db.blpop(ai_tts_key)
            # print(f"[AI TTS] Result from Redis: {result}")
            if result is None:
                continue
            else:
                _, ai_text_answer_bytes = result
                
            if ai_text_answer_bytes is None:
                continue
            
            if isinstance(ai_text_answer_bytes, bytes):
                ai_answer_str = ai_text_answer_bytes.decode('utf-8')
                
            if ai_answer_str == SIGNAL_AI_TTS_START:
                await client_ws.send_text(SIGNAL_AI_TTS_START)
            elif ai_answer_str == SIGNAL_AI_TTS_END:
                await client_ws.send_text(SIGNAL_AI_TTS_END)
                break
            else:
                # Determine the segment language for TTS synthesis
                lang = detect(ai_answer_str)
                print(f"language detected for TTS: {lang}")
                config = LANGUAGE_MAP.get(lang)
                if config is not None:
                    lang_path =config.path
                    sample_rate = config.sample_rate
                    config_path = lang_path + ".json"
                    voice = PiperVoice.load(lang_path, config_path)

                    for chunk in voice.synthesize(ai_answer_str):
                        audio_bytes = chunk.audio_int16_bytes
                        audio_bytes = resample_audio(audio_bytes, sample_rate, 16_000)
                        for i in range(0, len(audio_bytes), TTS_CHUNK_SIZE):
                            await client_ws.send_bytes(audio_bytes[i:i + TTS_CHUNK_SIZE])
                            # print(f"[WORKER AI TTS] Sent audio chunk of size {len(audio_bytes[i:i + TTS_CHUNK_SIZE])} bytes for {just_id}")

    except ConnectionClosed as e:
        print(f"[WORKER AI TTS] WebSocket closed: {e}")
        
    except Exception as e:
        print(f"[WORKER AI TTS] Exception occurred: {e}")

    finally:
        print("[WORKER AI TTS] Worker finished")