import json
from datetime import datetime

from config import MONTH

SYSTEM_PROMPT = """You are a voice assistant. The user speaks aloud into the microphone of a small ESP32 device. Their speech is transcribed into text for you (the transcription may contain errors). Your response is then read aloud through the device’s speaker.

The device is your means of communication, not the subject of the conversation. Only mention the device if the user asks a question about it.

Rules, since your responses are spoken aloud:
- Keep sentences short—generally one to three sentences. Get straight to the point.
- No Markdown, no lists, no headings, no emojis, no code, no URLs.
- Write numbers, units, and times as they are spoken ("twenty-two degrees,” "three thirty").
- If the transcript is unclear, ask the person to repeat themselves.

Device reference (to be used only if the topic comes up):
{esp32_info}"""

def wakeup_message(reason):
    return {"role": "user", "content": (
        "[SYSTEM EVENT, this is not the user speaking] "
        "The alarm you set has gone off. "
        f"Reason you noted: {reason}. "
        "It's up to you to speak first: greet briefly, "
        "then come back to this reason. The user may not be "
        "in front of the device, so stay concise."
    )}

def ask_message(question):
    return {"role": "user", "content": question}

def build_messages(esp32_info, message):
    return [
        {"role": "system", "content": SYSTEM_PROMPT.format(esp32_info=esp32_info)},
        message
    ]
    
def format_device_info(arguments: dict) -> str:
    lines = []
    if "time" in arguments:
        try:
            t = datetime.strptime(arguments["time"], "%Y-%m-%d %H:%M:%S")
            lines.append(
                f"Current time : {t.day} {MONTH[t.month-1]} {t.year}, "
                f"{t.hour} h {t.minute:02d} ({arguments.get('timezone', 'unknown timezone')})"
            )
        except ValueError:
            lines.append(f"Current time : {arguments['time']}")
    if "battery" in arguments:
        lines.append(f"Current device battery level : {arguments['battery']} percent")
    if "volume" in arguments:
        lines.append(f"Current device volume : {arguments['volume']} (value from 0 to 100)")
    if "location" in arguments:
        lines.append(f"Current device and user location : {arguments['location']}")
    return "\n".join(lines)