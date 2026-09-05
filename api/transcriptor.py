from faster_whisper import WhisperModel
import numpy as np

class Transcriptor:
    def __init__(self, MODEL):
        self.model = WhisperModel(MODEL, device="cpu", compute_type="int8", )
    
    def transcribe(self, audio: bytes, language: str):
        audio = self._convert_binary_to_np(audio)
        
        segments, info = self.model.transcribe(audio, language=language)
        for segment in segments:
            text = segment.text.strip()
            # filter some segments
            if not text:
                continue
            duration = segment.end - segment.start

            if duration < 0.3:
                continue

            if segment.no_speech_prob > 0.6:
                continue
            yield text, info.language
             
    @staticmethod
    def _convert_binary_to_np(data) -> np.float32:
        audio = np.frombuffer(data, dtype=np.int16)
        audio = audio.astype(np.float32) / 32768.0
        return audio