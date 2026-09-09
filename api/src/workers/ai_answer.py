from fastapi import WebSocket

from services.redis_controller import RedisController
from config import (REDIS_KEY_PREFIX_AI_TEXT, SIGNAL_AI_TEXT_START, SIGNAL_AI_TEXT_END)
from simple_websocket.errors import ConnectionClosed


async def worker_ai_answer(just_id: str, client_ws: WebSocket, redis_controller : RedisController): # purpose: send ai tts audio answer to the client
    try:
        ai_text_key = REDIS_KEY_PREFIX_AI_TEXT + just_id
        
        while True:
            result = await redis_controller.blpop(ai_text_key)
            if result is None:
                break
            else:
                _, ai_text_answer_bytes = result
            if ai_text_answer_bytes is None:
                break
            if isinstance(ai_text_answer_bytes, bytes):
                answer_str = ai_text_answer_bytes.decode('utf-8')
            if answer_str == SIGNAL_AI_TEXT_START:
                await client_ws.send_text(SIGNAL_AI_TEXT_START)
            elif answer_str == SIGNAL_AI_TEXT_END:
                await client_ws.send_text(SIGNAL_AI_TEXT_END)
                break
            else:
                print(f"[AI ANSWER] Sending AI text answer: {answer_str}")
                await client_ws.send_text(answer_str)
    except ConnectionClosed as e:
        print(f"[AI ANSWER] WebSocket closed: {e}")

    finally:
        print("[AI ANSWER] Worker finished")