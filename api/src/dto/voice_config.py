from typing import NamedTuple
from dataclasses import dataclass

@dataclass(frozen=True)
class VoiceConfig:
    path: str
    sample_rate: int
    