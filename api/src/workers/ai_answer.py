from fastapi import WebSocket

from databases.redis_db import RedisDatabase
from config import (REDIS_KEY_PREFIX_AI_TEXT, SIGNAL_AI_TEXT_START, SIGNAL_AI_TEXT_END)
from simple_websocket.errors import ConnectionClosed

# purpose: send ai tts audio answer to the client
async def worker_ai_answer(just_id: str, client_ws: WebSocket, redis_db : RedisDatabase):
    try:
        ai_text_key = REDIS_KEY_PREFIX_AI_TEXT + just_id
        
        while True:
            result = await redis_db.blpop(ai_text_key)
            # print(f"[WORKER AI ANSWER] Result from Redis: {result}")
            if result is None:
                break
            else:
                _, ai_text_answer_bytes = result
            if ai_text_answer_bytes is None:
                # break
                continue
            if isinstance(ai_text_answer_bytes, bytes):
                answer_str = ai_text_answer_bytes.decode('utf-8')
            if answer_str == SIGNAL_AI_TEXT_START:
                await client_ws.send_text(SIGNAL_AI_TEXT_START)
            elif answer_str == SIGNAL_AI_TEXT_END:
                await client_ws.send_text(SIGNAL_AI_TEXT_END)
                break
            else:
                # print(f"[AI ANSWER] Sending AI text answer: {answer_str}")
                await client_ws.send_text(answer_str)
    except ConnectionClosed as e:
        print(f"[WORKER AI ANSWER] WebSocket closed: {e}")
    
    except Exception as e:
        print(f"[WORKER AI ANSWER] Exception occurred: {e}")

    finally:
        print("[WORKER AI ANSWER] Worker finished")