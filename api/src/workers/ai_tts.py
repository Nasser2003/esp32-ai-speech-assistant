from fastapi import WebSocket
from piper import PiperVoice
from simple_websocket.errors import ConnectionClosed
import asyncio

from services.lang_detector import LanguageDetector
from services.audio_process_utils import resample_audio
from databases.redis_db import RedisDatabase
from config import ( REDIS_KEY_PREFIX_AI_TTS, REDIS_KEY_PREFIX_TRANSCRIPTION, 
    SIGNAL_AI_TTS_START, SIGNAL_AI_TTS_END, LANGUAGE_MAP, TTS_CHUNK_SIZE )

# purpose: send ai text answer to the client via TTS audio stream
# buffer_queue: asyncio.Queue fed by main.py with BUFFER_FREE credits from the ESP32
async def worker_ai_tts(just_id, client_ws: WebSocket, redis_db: RedisDatabase,
                        buffer_queue: asyncio.Queue, language_detector: LanguageDetector) -> None: 
    try:
        ai_tts_key = REDIS_KEY_PREFIX_AI_TTS + just_id

        # Cache loaded voices per language to avoid reloading on every sentence
        voice_cache: dict[str, tuple[PiperVoice, int]] = {}
        counter = 0
        while True:
            counter += 1
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
                try:
                    lang = language_detector.detect(ai_answer_str)
                except Exception as e:
                    print(f"[WORKER AI TTS] Error detecting language: {e}")
                    lang = await redis_db.getKey(ai_tts_key + "_lang")
                if lang is None:
                    lang = "en"
                config = LANGUAGE_MAP.get(str(lang)) if str(lang) in LANGUAGE_MAP else LANGUAGE_MAP.get("en")
                
                if config is None:
                    continue
                
                # Load the voice model only once per language (cache it)
                if str(lang) not in voice_cache:
                    lang_path = config.path
                    config_path = lang_path + ".json"
                    voice_cache[str(lang)] = (PiperVoice.load(lang_path, config_path), config.sample_rate)

                voice, sample_rate = voice_cache[str(lang)]

                for chunk in voice.synthesize(ai_answer_str):
                    audio_bytes = chunk.audio_int16_bytes
                    audio_bytes = resample_audio(audio_bytes, sample_rate, 16_000)
                    for i in range(0, len(audio_bytes), TTS_CHUNK_SIZE):
                        pcm_chunk = audio_bytes[i:i + TTS_CHUNK_SIZE]
                        # Wait for a backpressure credit from the ESP32
                        # (BUFFER_FREE signal routed via main.py ? buffer_queue)
                        await buffer_queue.get()
                        await client_ws.send_bytes(pcm_chunk)

    except ConnectionClosed as e:
        print(f"[WORKER AI TTS] WebSocket closed: {e}")
        
    except Exception as e:
        print(f"[WORKER AI TTS] Exception occurred: {e}")

    finally:
        print("[WORKER AI TTS] Worker finished")