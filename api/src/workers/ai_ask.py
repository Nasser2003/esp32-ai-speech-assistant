import ollama
from databases.redis_db import RedisDatabase
from config import (REDIS_KEY_PREFIX_TRANSCRIPTION, SIGNAL_AI_WAKE_UP, SIGNAL_TRANSCRIPTION_START, 
    SIGNAL_TRANSCRIPTION_END, REDIS_KEY_PREFIX_AI_TTS, 
    REDIS_KEY_PREFIX_AI_TEXT, SIGNAL_AI_TTS_START, SIGNAL_AI_TTS_END, 
    SIGNAL_AI_TEXT_START, SIGNAL_AI_TEXT_END, OLLAMA_CHAT_MODEL)
from services.ollama_service import ask_ai
from simple_websocket.errors import ConnectionClosed
from fastapi import WebSocket
import services.ollama_service as ollama_service

# purpose: process transcription depending on window size
async def worker_ai_ask(just_id: str, client_ws: WebSocket, redis_db: RedisDatabase, 
                     client_ia: ollama.AsyncClient, esp32_info: str = ""):
    try:
        trans_key = REDIS_KEY_PREFIX_TRANSCRIPTION + just_id
        ai_tts_key = REDIS_KEY_PREFIX_AI_TTS + just_id
        ai_text_key = REDIS_KEY_PREFIX_AI_TEXT + just_id
        question = ""
        

        while True:
            # check for transcriptions
            result = await redis_db.blpop(trans_key)
            
            IS_AI_WAKE_UP_CONTEXT = False
            
            if result is None:
                break
            else:
                _, transc_bytes = result
                
            if transc_bytes is None:
                break
            if isinstance(transc_bytes, bytes):
                transcription_str = transc_bytes.decode('utf-8')
                print(f"[WORKER AI ASK] Transcription element: {transcription_str}")
                IS_AI_WAKE_UP_CONTEXT = transcription_str.startswith(SIGNAL_AI_WAKE_UP)
            if transcription_str == SIGNAL_TRANSCRIPTION_START:
                await client_ws.send_text(SIGNAL_TRANSCRIPTION_START)
            elif transcription_str == SIGNAL_TRANSCRIPTION_END:
                await client_ws.send_text(SIGNAL_TRANSCRIPTION_END)
                break
            elif IS_AI_WAKE_UP_CONTEXT:
                question = transcription_str.split(":", 1)[1]
                await client_ws.send_text(SIGNAL_AI_WAKE_UP)
                break
            else:
                id_seg, text = transcription_str.split(":", 1)
                question += f"{text} "
                await client_ws.send_text(text)
        
        await redis_db.r_push_expire(ai_text_key, SIGNAL_AI_TEXT_START)
        await redis_db.r_push_expire(ai_tts_key, SIGNAL_AI_TTS_START)
        
        if IS_AI_WAKE_UP_CONTEXT:
            full_question = ollama_service.wakeup_message(question)
        else:
            full_question = ollama_service.ask_message(question)
        
        async for sentense in ask_ai(client_ia, OLLAMA_CHAT_MODEL, full_question, esp32_info):
            if sentense and sentense.strip():
                print(f"[WORKER AI ASK] AI answer: {sentense}")
                await redis_db.r_push_expire(ai_text_key, sentense)
                await redis_db.r_push_expire(ai_tts_key, sentense)
            
        await redis_db.r_push_expire(ai_text_key, SIGNAL_AI_TEXT_END)
        await redis_db.r_push_expire(ai_tts_key, SIGNAL_AI_TTS_END)
    except ConnectionClosed as e:
        print(f"[WORKER AI ASK] WebSocket closed: {e}")
    
    except Exception as e:
        print(f"[WORKER AI ASK] Exception occurred: {e}")

    finally:
        print("[WORKER AI ASK] Worker finished")
    
    