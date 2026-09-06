from piper import PiperVoice
import io
import wave

                
def create_wav_file(audio_bytes, filename: str):
    wav_buffer = io.BytesIO()

    with wave.open(wav_buffer, "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)  # 16 bits = 2 bytes
        wav.setframerate(16_000)
        wav.writeframes(audio_bytes)

    wav_bytes = wav_buffer.getvalue()
    with open(f"data/{filename}", "wb") as f:
                f.write(wav_bytes)

def test_received_audio(audio_bytes):
    create_wav_file(audio_bytes, "test_received_audio.wav")

def test_audio_tts(LANGUAGE_MAP, VOICE_LANGUAGE):
    lang_path = LANGUAGE_MAP.get(VOICE_LANGUAGE)
    config_path = LANGUAGE_MAP.get(VOICE_LANGUAGE) + ".json"
    voice = PiperVoice.load(lang_path, config_path)
    audio_bytes = b""

    for chunk in voice.synthesize("Hello, I am a voice assistant. How can I help you today?"):
    # for chunk in voice.synthesize("Hi, I am a voice assistant. How can I help you today?"):
        audio_bytes += chunk.audio_int16_bytes

    # create a WAV file from the audio bytes
    create_wav_file(audio_bytes, "test_tts.wav")