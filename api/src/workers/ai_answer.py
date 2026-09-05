from src.config import (PREFIX_AI_TEXT, AI_TEXT_START, AI_TEXT_END, TTL_EXPIRE_TIME)
from simple_websocket.errors import ConnectionClosed


async def worker_ai_answer(just_id, ws, redis_controller): # purpose: send ai tts audio answer to the client
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

    finally:
        print("[AI ANSWER] Worker finished")