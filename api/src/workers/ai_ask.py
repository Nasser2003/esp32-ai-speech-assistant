import traceback

import ollama
from services.ai_tools import AiToolsManager
from models.task import Task
from models.ai_context import RoleEnum
from services.ai_context_utils import (
    build_messages, format_device_info, build_wakeup_messages,
    build_messages_with_context, build_wakeup_messages_with_context, save_message
)
from databases.redis_db import RedisDatabase
from config import (REDIS_KEY_PREFIX_TRANSCRIPTION, SIGNAL_AI_WAKE_UP, SIGNAL_ARGUMENT, SIGNAL_TRANSCRIPTION_START, 
    SIGNAL_TRANSCRIPTION_END, REDIS_KEY_PREFIX_AI_TTS, 
    REDIS_KEY_PREFIX_AI_TEXT, SIGNAL_AI_TTS_START, SIGNAL_AI_TTS_END, 
    SIGNAL_AI_TEXT_START, SIGNAL_AI_TEXT_END, OLLAMA_CHAT_MODEL, MAX_CONTEXT_MESSAGES)
from services.ollama_service import ask_ai
from simple_websocket.errors import ConnectionClosed
from fastapi import WebSocket
import services.ollama_service as ollama_service
import json
from datetime import datetime
from sqlalchemy.orm import Session


# purpose: process transcription depending on window size
async def worker_ai_ask(just_id: str, client_ws: WebSocket, redis_db: RedisDatabase, 
                     client_ia: ollama.AsyncClient, postgres_db: Session, ai_tool_manager: AiToolsManager):
    try:
        trans_key = REDIS_KEY_PREFIX_TRANSCRIPTION + just_id
        ai_tts_key = REDIS_KEY_PREFIX_AI_TTS + just_id
        ai_text_key = REDIS_KEY_PREFIX_AI_TEXT + just_id
        argument_id = SIGNAL_ARGUMENT + just_id
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
                IS_AI_WAKE_UP_CONTEXT = transcription_str == SIGNAL_AI_WAKE_UP
            if transcription_str == SIGNAL_TRANSCRIPTION_START:
                await client_ws.send_text(SIGNAL_TRANSCRIPTION_START)
            elif transcription_str == SIGNAL_TRANSCRIPTION_END:
                await client_ws.send_text(SIGNAL_TRANSCRIPTION_END)
                break
            elif IS_AI_WAKE_UP_CONTEXT:
                await client_ws.send_text(SIGNAL_AI_WAKE_UP)
                break
            else:
                id_seg, text = transcription_str.split(":", 1)
                question += f"{text} "
                await client_ws.send_text(text)
        
        # reading stored arguments for the current session
        argument_data = await redis_db.getKey(argument_id)
        if argument_data is None:
            raise ValueError(f"No argument data found for key: {argument_id}")
        if isinstance(argument_data, bytes): # convert bytes|string to string to make the python interpreter happy
            argument_data = argument_data.decode('utf-8')
            arguments = json.loads(argument_data)
        
        IS_AI_WAKE_UP_CONTEXT = arguments.get("type") == SIGNAL_AI_WAKE_UP
        esp32_info = format_device_info(arguments)
        ai_tool_manager.set_esp32_info(arguments)
        
        device_mac = arguments.get("mac")
        system_id = None
        user_prompt_to_save = None

        if IS_AI_WAKE_UP_CONTEXT:
            reason = postgres_db.query(Task.argument) \
                .filter(Task.id == arguments.get("task_id")) \
                .scalar()
            full_question, system_id = build_wakeup_messages_with_context(
                postgres_db, esp32_info, reason, limit=MAX_CONTEXT_MESSAGES, device=device_mac
            )
            user_prompt_to_save = f"[Wake-up topic]: {reason}" if reason else "[Wake-up interaction]"
        else:
            full_question, system_id = build_messages_with_context(
                postgres_db, esp32_info, question, limit=MAX_CONTEXT_MESSAGES, device=device_mac
            )
            user_prompt_to_save = question
            
        await redis_db.r_push_expire(ai_text_key, SIGNAL_AI_TEXT_START)
        await redis_db.r_push_expire(ai_tts_key, SIGNAL_AI_TTS_START)
        
        full_ai_answer = ""
        async for sentense in ask_ai(client_ia, OLLAMA_CHAT_MODEL, full_question, ai_tool_manager):
            if sentense and sentense.strip():
                print(f"[WORKER AI ASK] AI answer: {sentense}")
                full_ai_answer += sentense
                await redis_db.r_push_expire(ai_text_key, sentense)
                await redis_db.r_push_expire(ai_tts_key, sentense)
            
        await redis_db.r_push_expire(ai_text_key, SIGNAL_AI_TEXT_END)
        await redis_db.r_push_expire(ai_tts_key, SIGNAL_AI_TTS_END)

        # Save conversation turn to PostgreSQL
        if system_id is not None:
            if user_prompt_to_save and user_prompt_to_save.strip():
                save_message(
                    postgres_db,
                    system_id=system_id,
                    role=RoleEnum.USER,
                    content=user_prompt_to_save,
                    device=device_mac,
                )
            if full_ai_answer and full_ai_answer.strip():
                save_message(
                    postgres_db,
                    system_id=system_id,
                    role=RoleEnum.ASSISTANT,
                    content=full_ai_answer,
                    device=device_mac,
                )

    except ConnectionClosed as e:
        print(f"[WORKER AI ASK] WebSocket closed: {e}")
    
    except Exception as e:
        print("[WORKER AI ASK] Exception occurred:", flush=True)
        traceback.print_exc()
        raise

    finally:
        print("[WORKER AI ASK] Worker finished")
    
    