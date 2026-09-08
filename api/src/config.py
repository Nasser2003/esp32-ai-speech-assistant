import os
from dotenv import load_dotenv


load_dotenv()


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

PREFIX_LANGUAGE = os.getenv("PREFIX_LANGUAGE")
PREFIX_RECORD = os.getenv("PREFIX_RECORD")
PREFIX_TRANSCRIPTION = os.getenv("PREFIX_TRANSCRIPTION")
PREFIX_AI_TEXT = os.getenv("PREFIX_AI_TEXT")
PREFIX_AI_TTS = os.getenv("PREFIX_AI_TTS")

RECORDING_START = os.getenv("RECORDING_START")
RECORDING_END = os.getenv("RECORDING_END")

TRANSCRIPTION_START = os.getenv("TRANSCRIPTION_START")
TRANSCRIPTION_END = os.getenv("TRANSCRIPTION_END")

AI_TEXT_START = os.getenv("AI_TEXT_START")
AI_TEXT_END = os.getenv("AI_TEXT_END")

AI_TTS_START = os.getenv("AI_TTS_START")
AI_TTS_END = os.getenv("AI_TTS_END")

WEBSOCKET_CLOSE = os.getenv("WEBSOCKET_CLOSE")