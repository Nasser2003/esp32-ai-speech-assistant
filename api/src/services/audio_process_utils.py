import numpy as np
from scipy.signal import resample_poly

def resample_audio(audio_bytes, original_rate: int, target_rate: int) -> bytes:
    if (original_rate == target_rate):
        return audio_bytes
    
    pcm = np.frombuffer(
        audio_bytes,
        dtype=np.int16
    )

    pcm_16k = resample_poly(
        pcm,
        target_rate,
        original_rate
    ).astype(np.int16)

    audio_bytes = pcm_16k.tobytes()
    
    return audio_bytes