from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import StreamingResponse
import ollama
import shortuuid
import asyncio

from config import (REDIS_TTL_EXPIRE_TIME, REDIS_HOST, REDIS_KEY_PREFIX_RECORD, 
    SIGNAL_RECORDING_START, SIGNAL_RECORDING_END, OLLAMA_CHAT_MODEL, SIGNAL_WEBSOCKET_CLOSE, 
    TRANSCRIPTION_MODEL, REDIS_PORT, OLLAMA_URL, API_WEBSOCKET_PATH)
from models.question import Question
from services.ollama_service import ask_ai
from services.redis_controller import RedisController
from services.transcriptor import Transcriptor
from workers.transcribe import worker_transcribe
from workers.ai_ask import worker_ask
from workers.ai_tts import worker_ai_tts
from workers.ai_answer import worker_ai_answer

app = FastAPI()

client_ia = ollama.Client(OLLAMA_URL)
redis_controller = RedisController(host=REDIS_HOST, port=REDIS_PORT, db=0, ttl=REDIS_TTL_EXPIRE_TIME)
transcriptor = Transcriptor(TRANSCRIPTION_MODEL)

@app.post('/ask')
def ask(question: Question):
    # # send prompt to the model
    return StreamingResponse(
        ask_ai(client_ia, OLLAMA_CHAT_MODEL, question.question),
        media_type="text/plain"
    )


@app.get('/')
def home():
    return "Welcome to the AI Question Answering API!"


@app.websocket(API_WEBSOCKET_PATH)
async def websocket(client_ws: WebSocket):
    just_id = shortuuid.uuid()
    audio_stream_id = REDIS_KEY_PREFIX_RECORD + just_id
    
    try:
        await client_ws.accept()
        while True:
            message = await client_ws.receive()
            IS_SIGNAL ="text" in message
            IS_BYTES = "bytes" in message
            IS_RECORDING_START = IS_SIGNAL and message["text"] == SIGNAL_RECORDING_START
            IS_RECORDING_END = IS_SIGNAL and message["text"] == SIGNAL_RECORDING_END
            IS_WEBSOCKET_CLOSE = IS_SIGNAL and \
                (message["text"] == SIGNAL_WEBSOCKET_CLOSE or message["type"] == "websocket.disconnect")
            
            if message is None:
                break
            if IS_RECORDING_START:
                print(f"[WS] Recording started for {just_id}", flush=True)
                await redis_controller.r_push_expire(audio_stream_id, SIGNAL_RECORDING_START)  # Push a value to indicate the end of the session
                # thread for audio transcription
                asyncio.create_task(worker_transcribe(just_id, redis_controller, transcriptor))
                # thread for asking ai
                asyncio.create_task(worker_ask(just_id, client_ws, redis_controller, client_ia))
                # thread for sending ai tts audio
                asyncio.create_task(worker_ai_tts(just_id, client_ws, redis_controller))
                # thread for sending ai answer
                asyncio.create_task(worker_ai_answer(just_id, client_ws, redis_controller))
            elif IS_RECORDING_END:
                print(f"[WS] Recording ended for {just_id}", flush=True)
                await redis_controller.r_push_expire(audio_stream_id, SIGNAL_RECORDING_END)  # Push a value to indicate the end of the session
            elif IS_WEBSOCKET_CLOSE:
                print(f"[WS] WebSocket close signal received for {just_id}", flush=True)
                await client_ws.close()
                break
            elif IS_BYTES:
                audio_bytes = message["bytes"]
                print(f"[WS] Received audio bytes for {just_id}, length: {len(audio_bytes)}", flush=True)
                await redis_controller.r_push_expire(audio_stream_id, audio_bytes)
            else:
                print(f"[WS] Received unknown message type for {just_id}: {message}", flush=True)
    except WebSocketDisconnect:
        print(f"[WS] Client disconnected: {just_id}")

    except Exception as e:
        print(f"[WS] Unexpected error for {just_id}: {e}")
    finally:
        pass
