# constants
url = "127.0.0.1:11434/api/chat"
TTL_EXPIRE_TIME = 120  # 5 min sec
TRANSCRIPTION_CHUNK_SIZE = 64_000 # 2 seconds of audio at 16kHz, 16-bit PCM
TRANSCRIPTION_WINDOW_SIZE = 32_000 # clide every 1 second of audio
TRANS_MODEL = "small"
CHAT_MODEL = "llama3.2"

# constant strings
PREFIX_LANGUAGE = "audio_language:" # recorded audio
PREFIX_RECORD = "audio_record:" # recorded audio
PREFIX_TRANSCRIPTION = "audio_trans:" # transcription
PREFIX_AI_TEXT = "ai_text:" # ai text response
PREFIX_AI_TTS = "ai_tts:" # ai audio response

RECORDING_START = "/RECORDING START" # start recording audio stream
RECORDING_END = "/RECORDING END"
TRANSCRIPTION_START = "/TRANSCRIPTION START" # start transcription of the audio stream
TRANSCRIPTION_END = "/TRANSCRIPTION END"
AI_TEXT_START = "/AI TEXT START" # start sending ai textual answer to the client
AI_TEXT_END = "/AI TEXT END"
AI_TTS_START = "/AI TTS START" # start sending ai audio answer to the client
AI_TTS_END = "/AI TTS END"
WEBSOCKET_CLOSE = "/WEBSOCKET CLOSE"

VOICE_LANGUAGE = "auto" # "en" or "fr" or "auto"
LANGUAGE_MAP = {
    "en": ["data/en_US-ryan-low.onnx", 16_000],
    "fr": ["data/fr_FR-upmc-medium.onnx", 22_050],
    "ru": ["data/ru_RU-ruslan-medium.onnx", 22_050],
}