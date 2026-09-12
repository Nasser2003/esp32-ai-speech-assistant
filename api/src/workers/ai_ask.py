import ollama
from services.redis_controller import RedisController
from config import (REDIS_KEY_PREFIX_TRANSCRIPTION, SIGNAL_TRANSCRIPTION_START, 
    SIGNAL_TRANSCRIPTION_END, REDIS_KEY_PREFIX_AI_TTS, 
    REDIS_KEY_PREFIX_AI_TEXT, SIGNAL_AI_TTS_START, SIGNAL_AI_TTS_END, 
    SIGNAL_AI_TEXT_START, SIGNAL_AI_TEXT_END, OLLAMA_CHAT_MODEL)
from services.ollama_service import ask_ai
from simple_websocket.errors import ConnectionClosed
from fastapi import WebSocket

# purpose: process transcription depending on window size
async def worker_ask(just_id: str, client_ws: WebSocket, redis_controller: RedisController, 
                     client_ia: ollama.AsyncClient):
    try:
        trans_key = REDIS_KEY_PREFIX_TRANSCRIPTION + just_id
        ai_tts_key = REDIS_KEY_PREFIX_AI_TTS + just_id
        ai_text_key = REDIS_KEY_PREFIX_AI_TEXT + just_id
        question = ""

        while True:
            # check for transcriptions
            result = await redis_controller.blpop(trans_key)
            if result is None:
                break
            else:
                _, transc_bytes = result
                
            if transc_bytes is None:
                break
            if isinstance(transc_bytes, bytes):
                transcription_str = transc_bytes.decode('utf-8')
                print(f"[WEBSOCKET] Transcription element: {transcription_str}")
            if transcription_str == SIGNAL_TRANSCRIPTION_START:
                await client_ws.send_text(SIGNAL_TRANSCRIPTION_START)
            elif transcription_str == SIGNAL_TRANSCRIPTION_END:
                await client_ws.send_text(SIGNAL_TRANSCRIPTION_END)
                break
            else:
                id_seg, text = transcription_str.split(":", 1)
                question += f"{text} "
                await client_ws.send_text(text)
        
        await redis_controller.r_push_expire(ai_tts_key, SIGNAL_AI_TTS_START)
        await redis_controller.r_push_expire(ai_text_key, SIGNAL_AI_TEXT_START)
        
        async for sentense in ask_ai(client_ia, OLLAMA_CHAT_MODEL, question):
            if sentense and sentense.strip():
                print(f"[AI ASK] AI answer: {sentense}")
                await redis_controller.r_push_expire(ai_tts_key, sentense)
                await redis_controller.r_push_expire(ai_text_key, sentense)
            
        await redis_controller.r_push_expire(ai_tts_key, SIGNAL_AI_TTS_END)
        await redis_controller.r_push_expire(ai_text_key, SIGNAL_AI_TEXT_END)
    except ConnectionClosed as e:
        print(f"[AI ASK] WebSocket closed: {e}")

    finally:
        print("[AI ASK] Worker finished")
    
    