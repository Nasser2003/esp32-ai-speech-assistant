from groq import Groq
import io
import wave


class Transcriptor:
    def __init__(self, model: str, api_key: str):
        self.client = Groq(api_key=api_key)
        self.model = model

    def transcribe(self, audio: bytes, language: str):
        # Ton audio est actuellement du PCM16 brut.
        # Groq attend un fichier audio, donc on l'encapsule en WAV.
        wav_buffer = io.BytesIO()

        with wave.open(wav_buffer, "wb") as wav_file:
            wav_file.setnchannels(1)
            wav_file.setsampwidth(2)  # int16 = 2 bytes
            wav_file.setframerate(16000)
            wav_file.writeframes(audio)

        wav_buffer.seek(0)

        transcription = self.client.audio.transcriptions.create(
            file=("audio.wav", wav_buffer.read()),
            model=self.model,
            language=language,
            response_format="verbose_json",
            temperature=0.0,
        )

        # Pour conserver une interface proche de faster-whisper
        text = transcription.text.strip()
        print(f"[GROQ TRANSCRIBER] Transcription element: {transcription}")
        if text:
            yield text, language