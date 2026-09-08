from src.models.question import Question
from fastapi import FastAPI, WebSocket
from fastapi.responses import StreamingResponse
import ollama
from src.services.ollama_service import ask_ai
from src.services.redis_controller import RedisController
import shortuuid
from src.services.transcriptor import Transcriptor
from src.config import (TTL_EXPIRE_TIME, REDIS_PREFIX_RECORD, RECORDING_START, 
    RECORDING_END, CHAT_MODEL, WEBSOCKET_CLOSE, TRANS_MODEL, REDIS_PORT, OLLAMA_URL)
from src.workers.transcribe import worker_transcribe
from src.workers.ai_ask import worker_ask
from src.workers.ai_tts import worker_ai_tts
from src.workers.ai_answer import worker_ai_answer
import asyncio

app = FastAPI()

client = ollama.Client(OLLAMA_URL)
redis_controller = RedisController(host='localhost', port=REDIS_PORT, db=0)
transcriptor = Transcriptor(TRANS_MODEL)

@app.post('/ask')
def ask(question: Question):
    # # send prompt to the model
    return StreamingResponse(
        ask_ai(client, CHAT_MODEL, question.question),
        media_type="text/plain"
    )


@app.get('/')
def home():
    return "Welcome to the AI Question Answering API!"


@app.websocket("/ws/esp32")
async def websocket(client_ws: WebSocket):
    just_id = shortuuid.uuid()
    audio_stream_id = REDIS_PREFIX_RECORD + just_id
    await client_ws.accept()
    
    while True:
        message = await client_ws.receive()
        IS_SIGNAL = isinstance(message, str)
        IS_BYTES = isinstance(message, bytes)
        IS_RECORDING_START = IS_SIGNAL and message.startswith(RECORDING_START)
        IS_RECORDING_END = IS_SIGNAL and message.startswith(RECORDING_END)
        IS_WEBSOCKET_CLOSE = IS_SIGNAL and message.startswith(WEBSOCKET_CLOSE)
        
        if message is None:
            break
        if IS_RECORDING_START:
            await redis_controller.r_push_expire(audio_stream_id, RECORDING_START, TTL_EXPIRE_TIME)  # Push a value to indicate the end of the session
            # thread for audio transcription
            asyncio.create_task(worker_transcribe(just_id, redis_controller, transcriptor))
            # thread for asking ai
            asyncio.create_task(worker_ask(just_id, client_ws))
            # thread for sending ai tts audio
            asyncio.create_task(worker_ai_tts(just_id, client_ws))
            # thread for sending ai answer
            asyncio.create_task(worker_ai_answer(just_id, client_ws))
        elif IS_RECORDING_END:
            await redis_controller.r_push_expire(audio_stream_id, RECORDING_END, TTL_EXPIRE_TIME)  # Push a value to indicate the end of the session
        elif IS_WEBSOCKET_CLOSE:
            await client_ws.close()
        elif IS_BYTES:
            await redis_controller.r_push_expire(audio_stream_id, message, TTL_EXPIRE_TIME)