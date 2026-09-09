from fastapi import WebSocket
from piper import PiperVoice
from simple_websocket.errors import ConnectionClosed

from services.audio_process_utils import resample_audio
from services.redis_controller import RedisController
from config import ( REDIS_KEY_PREFIX_AI_TTS, SIGNAL_AI_TTS_START, SIGNAL_AI_TTS_END, 
    LANGUAGE_MAP, REDIS_KEY_PREFIX_LANGUAGE )

# purpose: send ai text answer to the client
async def worker_ai_tts(just_id, client_ws: WebSocket, redis_controller: RedisController): 
    try:
        ai_tts_key = REDIS_KEY_PREFIX_AI_TTS + just_id
        
        while True:
            result = await redis_controller.blpop(ai_tts_key)
            if result is None:
                break
            else:
                _, ai_text_answer_bytes = result
                
            if ai_text_answer_bytes is None:
                break
            
            if isinstance(ai_text_answer_bytes, bytes):
                ai_answer_str = ai_text_answer_bytes.decode('utf-8')
                
            if ai_answer_str == SIGNAL_AI_TTS_START:
                await client_ws.send_text(SIGNAL_AI_TTS_START)
            elif ai_answer_str == SIGNAL_AI_TTS_END:
                await client_ws.send_text(SIGNAL_AI_TTS_END)
                break
            else:
                # Determine the segment language for TTS synthesis
                lang = await redis_controller.get_majoritary(REDIS_KEY_PREFIX_LANGUAGE + just_id)
                await redis_controller.setTTL(REDIS_KEY_PREFIX_LANGUAGE + just_id)
                if lang is None:
                    lang = "en"
                config = LANGUAGE_MAP.get(lang)
                if config is not None:
                    lang_path =config.path
                    sample_rate = config.sample_rate
                    config_path = lang_path + ".json"
                    voice = PiperVoice.load(lang_path, config_path)

                    for chunk in voice.synthesize(ai_answer_str):
                        audio_bytes = chunk.audio_int16_bytes
                        audio_bytes = resample_audio(audio_bytes, sample_rate, 16_000)
                        await client_ws.send_bytes(audio_bytes)

    except ConnectionClosed as e:
        print(f"[AI TTS] WebSocket closed: {e}")

    finally:
        print("[AI TTS] Worker finished")