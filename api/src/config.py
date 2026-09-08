import os
from dotenv import load_dotenv

# This line is not necesary because podman compose is injecting itself the 
#   variables into the container thanks to this part in docker-compose.yml:
# service: 
#   api:
#     env_file:
#       - .env
# load_dotenv() 


OLLAMA_URL = os.getenv("OLLAMA_URL")
OLLAMA_CHAT_PATH = os.getenv("OLLAMA_CHAT_PATH")

TTL_EXPIRE_TIME = int(os.getenv("TTL_EXPIRE_TIME"))
TRANSCRIPTION_CHUNK_SIZE = int(os.getenv("TRANSCRIPTION_CHUNK_SIZE"))
TRANSCRIPTION_WINDOW_SIZE = int(os.getenv("TRANSCRIPTION_WINDOW_SIZE"))

TRANS_MODEL = os.getenv("TRANS_MODEL")
CHAT_MODEL = os.getenv("CHAT_MODEL")

REDIS_PORT = int(os.getenv("REDIS_PORT"))
API_PORT = int(os.getenv("API_PORT"))


VOICE_LANGUAGE = os.getenv("VOICE_LANGUAGE")
LANGUAGE_MAP = {
    "en": ["data/en_US-ryan-low.onnx", 16_000],
    "fr": ["data/fr_FR-upmc-medium.onnx", 22_050],
    "ru": ["data/ru_RU-ruslan-medium.onnx", 22_050],
}

REDIS_PREFIX_LANGUAGE = os.getenv("REDIS_PREFIX_LANGUAGE")
REDIS_PREFIX_RECORD = os.getenv("REDIS_PREFIX_RECORD")
REDIS_PREFIX_TRANSCRIPTION = os.getenv("REDIS_PREFIX_TRANSCRIPTION")
REDIS_PREFIX_AI_TEXT = os.getenv("REDIS_PREFIX_AI_TEXT")
REDIS_PREFIX_AI_TTS = os.getenv("REDIS_PREFIX_AI_TTS")

RECORDING_START = os.getenv("RECORDING_START")
RECORDING_END = os.getenv("RECORDING_END")

TRANSCRIPTION_START = os.getenv("TRANSCRIPTION_START")
TRANSCRIPTION_END = os.getenv("TRANSCRIPTION_END")

AI_TEXT_START = os.getenv("AI_TEXT_START")
AI_TEXT_END = os.getenv("AI_TEXT_END")

AI_TTS_START = os.getenv("AI_TTS_START")
AI_TTS_END = os.getenv("AI_TTS_END")

WEBSOCKET_CLOSE = os.getenv("WEBSOCKET_CLOSE")