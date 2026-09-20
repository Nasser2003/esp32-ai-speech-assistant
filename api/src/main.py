from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.responses import StreamingResponse
import ollama
import shortuuid
import asyncio
import time
import wave
from pathlib import Path
from sqlalchemy.orm import Session
from typing import Annotated
from fastapi import Depends
from datetime import datetime, timezone

from dto.task_dto import TaskUpdate
from databases.postgres_db import PostgresDatabase
from config import (POSTGRES_DB, POSTGRES_HOST, POSTGRES_PASSWORD, POSTGRES_PORT, POSTGRES_USER, REDIS_TTL_EXPIRE_TIME, REDIS_HOST, REDIS_KEY_PREFIX_RECORD, 
    SIGNAL_RECORDING_START, SIGNAL_RECORDING_END, OLLAMA_CHAT_MODEL, 
    TRANSCRIPTION_MODEL, REDIS_PORT, OLLAMA_URL, API_WEBSOCKET_PATH,
    VOICE_LANGUAGE)
from dto.question import Question
from services.ollama_service import ask_ai
from databases.redis_db import RedisDatabase
from services.transcriptor import Transcriptor
from workers.transcribe import worker_transcribe
from workers.ai_ask import worker_ai_ask
from workers.ai_tts import worker_ai_tts
from workers.ai_answer import worker_ai_answer
from workers.worker_supervisor import terminate_session_if_workers_done
from models.task import Task, TaskTypeEnum, TaskStatusEnum

app = FastAPI()

client_ia = ollama.AsyncClient(OLLAMA_URL)
redis_db = RedisDatabase(
    host=REDIS_HOST, port=REDIS_PORT, db=0, ttl=REDIS_TTL_EXPIRE_TIME
)
postgres_db = PostgresDatabase(
    host=POSTGRES_HOST, port=POSTGRES_PORT, user=POSTGRES_USER, 
    password=POSTGRES_PASSWORD, database=POSTGRES_DB
)
postgres_dep = Annotated[Session, Depends(postgres_db.get_session)]
transcriptor = Transcriptor(TRANSCRIPTION_MODEL)


@app.post('/ask')
def ask(question: Question):
    # # send prompt to the model
    return StreamingResponse(
        ask_ai(client_ia, OLLAMA_CHAT_MODEL, question.question),
        media_type="text/plain"
    )


@app.get("/current-task")
def get_current_task(db: postgres_dep):

    task = (
        db.query(Task)
        .filter(Task.type == TaskTypeEnum.ALARM)
        .filter(Task.status == TaskStatusEnum.PENDING)
        .filter(Task.run_at <= datetime.now(timezone.utc))
        .order_by(Task.run_at.asc())
        .first()
    )

    if not task:
        return {"error": "No current task found"}

    return {
        "id": task.id,
        "task_type": task.type,
        "run_at": task.run_at,
        "argument": task.argument
    }
    
@app.patch("/tasks/{task_id}")
def patch_current_task(task_id: int, task_status: TaskUpdate, db: postgres_dep):

    task = db.query(Task).filter(Task.id == task_id).first()

    if not task:
        raise HTTPException(
            status_code=404,
            detail="Task not found"
        )

    task.status = task_status.task_status
    db.commit()

    return {
        "id": task.id,
        "status": task.status
    }


@app.get('/')
def home():
    return "Welcome to the AI Question Answering API!"


@app.websocket(API_WEBSOCKET_PATH)
async def websocket(client_ws: WebSocket):
    just_id = shortuuid.uuid()
    audio_stream_id = REDIS_KEY_PREFIX_RECORD + just_id
    supervisor_task = None
    
    try:
        await client_ws.accept()
        while True:
            message = await client_ws.receive()
            IS_SIGNAL ="text" in message
            IS_BYTES = "bytes" in message
            IS_RECORDING_START = IS_SIGNAL and message["text"] == SIGNAL_RECORDING_START
            IS_RECORDING_END = IS_SIGNAL and message["text"] == SIGNAL_RECORDING_END
            IS_WEBSOCKET_CLOSE = (message["type"] == "websocket.disconnect")
            
            if message is None:
                break
            if IS_RECORDING_START:
                print(f"[WS] Recording started for {just_id}", flush=True)
                # Push a value to indicate the end of the session
                await redis_db.r_push_expire(audio_stream_id, SIGNAL_RECORDING_START)  
                worker_tasks = [
                    # thread for audio transcription
                    asyncio.create_task(worker_transcribe(just_id, redis_db, transcriptor)),
                    # thread for asking ai
                    asyncio.create_task(worker_ai_ask(just_id, client_ws, redis_db, client_ia)),
                    # thread for sending ai tts audio
                    asyncio.create_task(worker_ai_tts(just_id, client_ws, redis_db)),
                    # thread for sending ai answer
                    asyncio.create_task(worker_ai_answer(just_id, client_ws, redis_db)),
                ]
                supervisor_task = asyncio.create_task(
                    terminate_session_if_workers_done(worker_tasks, client_ws, just_id)
                )
            elif IS_RECORDING_END:
                print(f"[WS] Recording ended for {just_id}", flush=True)
                await redis_db.r_push_expire(audio_stream_id, SIGNAL_RECORDING_END)  # Push a value to indicate the end of the session
            elif IS_WEBSOCKET_CLOSE:
                print(f"[WS] WebSocket close signal received for {just_id}", flush=True)
                # await client_ws.close()
                break
            elif IS_BYTES:
                audio_bytes = message["bytes"]
                print(f"[WS] Received audio bytes for {just_id}, length: {len(audio_bytes)}", flush=True)
                await redis_db.r_push_expire(audio_stream_id, audio_bytes)
            else:
                print(f"[WS] Received unknown message type for {just_id}: {message}", flush=True)
    except WebSocketDisconnect:
        print(f"[WS] Client disconnected: {just_id}")

    except Exception as e:
        print(f"[WS] Unexpected error for {just_id}: {e}")
        
    finally:
        if supervisor_task is not None and not supervisor_task.done():
            supervisor_task.cancel()

# def test_transcription_from_wav():
#     wav_path = Path(__file__).resolve().parents[1] / "data" / "test_received_audio.wav"

#     with wave.open(str(wav_path), "rb") as wav_file:
#         audio_bytes = wav_file.readframes(wav_file.getnframes())

#     started_at = time.perf_counter()
#     transcriptions = []
#     for text, detected_language in transcriptor.transcribe(audio_bytes, "fr"):
#         transcriptions.append(text)
#         print(f"[{detected_language}] {text}")

#     elapsed = time.perf_counter() - started_at
#     print(f"Transcription: {' '.join(transcriptions)}")
#     print(f"Temps de transcription: {elapsed:.3f} s")


# # if __name__ == "__main__":
# test_transcription_from_wav()
    