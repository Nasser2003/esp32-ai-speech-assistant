import os
from models.voice_config import VoiceConfig


def getenv_checked(var_name: str) -> str:
    value = os.getenv(var_name)
    if value is None:
        raise ValueError(f"Environment variable '{var_name}' is not set.")
    return value

def getenv_checked_int(var_name: str) -> int:
    value_str = getenv_checked(var_name)
    try:
        return int(value_str)
    except ValueError:
        raise ValueError(f"Environment variable '{var_name}' must be an integer, got '{value_str}'.")
    
API_WEBSOCKET_PATH = getenv_checked("API_WEBSOCKET_PATH")
    
OLLAMA_URL = getenv_checked("OLLAMA_URL")
OLLAMA_CHAT_MODEL = getenv_checked("OLLAMA_CHAT_MODEL")

TRANSCRIPTION_CHUNK_SIZE = 120_000
TRANSCRIPTION_WINDOW_SIZE = getenv_checked_int("TRANSCRIPTION_WINDOW_SIZE")

TRANSCRIPTION_MODEL = "medium"

REDIS_HOST = getenv_checked("REDIS_HOST")
REDIS_PORT = getenv_checked_int("REDIS_PORT")
REDIS_TTL_EXPIRE_TIME = getenv_checked_int("REDIS_TTL_EXPIRE_TIME")
REDIS_KEY_PREFIX_LANGUAGE = getenv_checked("REDIS_KEY_PREFIX_LANGUAGE")
REDIS_KEY_PREFIX_RECORD = getenv_checked("REDIS_KEY_PREFIX_RECORD")
REDIS_KEY_PREFIX_TRANSCRIPTION = getenv_checked("REDIS_KEY_PREFIX_TRANSCRIPTION")
REDIS_KEY_PREFIX_AI_TEXT = getenv_checked("REDIS_KEY_PREFIX_AI_TEXT")
REDIS_KEY_PREFIX_AI_TTS = getenv_checked("REDIS_KEY_PREFIX_AI_TTS")

POSTGRES_HOST = getenv_checked("POSTGRES_HOST")
POSTGRES_PORT = getenv_checked_int("POSTGRES_PORT")
POSTGRES_DB = getenv_checked("POSTGRES_DB")
POSTGRES_USER = getenv_checked("POSTGRES_USER")
POSTGRES_PASSWORD = getenv_checked("POSTGRES_PASSWORD")

API_PORT = getenv_checked_int("API_PORT")

VOICE_LANGUAGE = getenv_checked("VOICE_LANGUAGE")
LANGUAGE_MAP: dict[str, VoiceConfig] = {
    "en": VoiceConfig("data/en_US-ryan-low.onnx", 16_000),
    "fr": VoiceConfig("data/fr_FR-upmc-medium.onnx", 22_050),
    "ru": VoiceConfig("data/ru_RU-ruslan-medium.onnx", 22_050),
}

SIGNAL_RECORDING_START = getenv_checked("SIGNAL_RECORDING_START")
SIGNAL_RECORDING_END = getenv_checked("SIGNAL_RECORDING_END")
SIGNAL_TRANSCRIPTION_START = getenv_checked("SIGNAL_TRANSCRIPTION_START")
SIGNAL_TRANSCRIPTION_END = getenv_checked("SIGNAL_TRANSCRIPTION_END")
SIGNAL_AI_TEXT_START = getenv_checked("SIGNAL_AI_TEXT_START")
SIGNAL_AI_TEXT_END = getenv_checked("SIGNAL_AI_TEXT_END")
SIGNAL_AI_TTS_START = getenv_checked("SIGNAL_AI_TTS_START")
SIGNAL_AI_TTS_END = getenv_checked("SIGNAL_AI_TTS_END")