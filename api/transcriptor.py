from faster_whisper import WhisperModel
import numpy as np

class Transcriptor:
    def __init__(self, MODEL):
        self.model = WhisperModel(MODEL, device="cpu", compute_type="int8", )
    
    def transcribe(self, audio: bytes):
        audio = self._convert_binary_to_np(audio)
        
        segments, _ = self.model.transcribe(audio, language="fr")
        for segment in segments:
            text = segment.text.strip()
            if not text:
                continue
            yield text
             
    @staticmethod
    def _convert_binary_to_np(data) -> np.float32:
        audio = np.frombuffer(data, dtype=np.int16)
        audio = audio.astype(np.float32) / 32768.0
        return audio