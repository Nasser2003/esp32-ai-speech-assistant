from src.models.question import Question
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import StreamingResponse
import ollama
from src.services.ollama_service import ask_ai
from src.services.redis_controller import RedisController
import threading
import shortuuid
from src.services.transcriptor import Transcriptor
from src.config import (TTL_EXPIRE_TIME, PREFIX_RECORD, RECORDING_START, 
    RECORDING_END, CHAT_MODEL, WEBSOCKET_CLOSE, TRANS_MODEL, REDIS_PORT,
    API_PORT, OLLAMA_URL)
from src.workers.transcribe import worker_transcribe
from src.workers.ai_ask import worker_ask
from src.workers.ai_tts import worker_ai_tts
from src.workers.ai_answer import worker_ai_answer

app = FastAPI()

# variables
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


# @sock.route("/ws")
def websocket(ws):
    just_id = shortuuid.uuid()
    audio_stream_id = PREFIX_RECORD + just_id
    
    while True:
        message = ws.receive()
        IS_SIGNAL = isinstance(message, str)
        IS_RECORDING_START = IS_SIGNAL and message.startswith(RECORDING_START)
        IS_RECORDING_END = IS_SIGNAL and message.startswith(RECORDING_END)
        IS_WEBSOCKET_CLOSE = IS_SIGNAL and message.startswith(WEBSOCKET_CLOSE)
        
        if message is None:
            break
        if IS_RECORDING_START:
            redis_controller.r_push_expire(audio_stream_id, RECORDING_START, TTL_EXPIRE_TIME)  # Push a value to indicate the end of the session
            # thread for audio transcription
            threading.Thread(
                target=worker_transcribe,
                args=(just_id,),
                daemon=True
            ).start()
            # thread for asking ai
            threading.Thread(
                target=worker_ask,
                args=(just_id, ws,),
                daemon=True
            ).start()
            # thread for sending ai tts audio
            threading.Thread(
                target=worker_ai_tts,
                args=(just_id, ws,),
                daemon=True
            ).start()
            # thread for sending ai answer
            threading.Thread(
                target=worker_ai_answer,
                args=(just_id, ws,),
                daemon=True
            ).start()
        if IS_RECORDING_END:
            redis_controller.r_push_expire(audio_stream_id, RECORDING_END, TTL_EXPIRE_TIME)  # Push a value to indicate the end of the session
        if IS_WEBSOCKET_CLOSE:
            ws.close()
        if isinstance(message, bytes):
            redis_controller.r_push_expire(audio_stream_id, message, TTL_EXPIRE_TIME)
    
if __name__ == '__main__':
    app.run(debug=True, host="0.0.0.0", port=API_PORT)