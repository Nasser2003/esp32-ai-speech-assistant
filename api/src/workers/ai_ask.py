from src.config import (PREFIX_TRANSCRIPTION, TRANSCRIPTION_START, TRANSCRIPTION_END, 
    TTL_EXPIRE_TIME, PREFIX_AI_TTS, PREFIX_AI_TEXT, AI_TTS_START, AI_TTS_END, 
    AI_TEXT_START, AI_TEXT_END, CHAT_MODEL)
from src.services.ollama_service import ask_ai
from simple_websocket.errors import ConnectionClosed

# purpose: process transcription depending on window size
async def worker_ask(just_id, ws, redis_controller, client):
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

    finally:
        print("[AI ASK] Worker finished")
    
    