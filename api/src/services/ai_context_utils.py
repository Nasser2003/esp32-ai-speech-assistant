import json
from datetime import datetime

MONTH = [
    "January", "February", "March", "April", "May", "June", "July",
    "August", "September", "October", "November", "December"
]

WEEKDAYS = [
    "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday", "Sunday",
]

SYSTEM_PROMPT = """You are a voice assistant. The user speaks aloud into the microphone of a small ESP32 device. Their speech is transcribed into text for you (the transcription may contain errors). Your response is then read aloud through the device's speaker.

The device is your means of communication, not the subject of the conversation. Only mention the device if the user asks a question about it. Since your Device is hold by the user, they may ask about time, location as "what time is it" or "where am I". You can answer these questions based on the device information provided in the context below.

You have access to tools that can perform actions for the user. If the user asks you to perform an action that one of your tools can perform, you MUST call the corresponding tool.
The user may ask you to perform actions in another language than English. Try to translate the user's language into tools.
Examples:

- If the user asks to set, create, define, or schedule an alarm, call createAlarm.
- If the user asks to change the volume, call changeVolume.
- If the user asks to continue or resume a conversation later, call continueConversationLater.

Rules, since your responses are spoken aloud:
- Keep sentences short—generally one to three sentences. Get straight to the point.
- No Markdown, no lists, no headings, no emojis, no code, no URLs.
- If the transcript is unclear, ask the person to repeat themselves.
- If a user asks you a question, answer with a sentence.
- The language of the response should match the language of the user's context.

Device reference (to be used only if the user asks about it, or if the topic about the device is relevant to the conversation):
{esp32_info}"""

PROACTIVE_ADDENDUM = """

You are initiating this conversation right now — the user has not spoken yet. Speak first, directly to them, as if you just thought of it yourself. Do not mention waking up, being reminded, or a notification. Do not ask "how can I help?".
Reason to bring this up: {reason}
Example of the tone expected: "Hey, did you end up deciding what you're doing this weekend?"
Say one or two short sentences."""

TOOLS_JSON = [
    {
        "type": "function",
        "function": {
            "name": "createAlarm",
            "description": (
                "Define an alarm for the user. "
                "ALWAYS call this function when the user asks to set, create, "
                "define, or schedule an alarm. "
                "If the user says 'now' or 'maintenant', set time to 'now'. "
                "Do not ask for the time when the user already said 'now'."
            ),
            "parameters": {
                "type": "object",
                "properties": {
                    "time": {
                        "type": "string",
                        "description": (
                            "Alarm time. Use 'now' when the user asks "
                            "for the alarm immediately."
                        )
                    }
                },
                "required": ["time"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "continueConversationLater",
            "description": (
                "Schedule a future proactive interaction with the user. "
                "Use this when the AI should independently contact the user later "
                "to continue, revisit, or follow up on a conversation or topic. "
                "At the scheduled time, the AI will initiate the interaction "
                "without waiting for the user to speak first. "
                "This is not merely a reminder: it creates a future conversation "
                "that the AI is responsible for starting. "
                "The interaction may happen silently if no spoken notification is needed."
            ),
            "parameters": {
                "type": "object",
                "properties": {
                    "time": {
                        "type": "string",
                        "description": (
                            "The local date and time when the AI should proactively "
                            "contact the user, in YYYY-MM-DD HH:MM format."
                        ),
                    },
                    "reason": {
                        "type": "string",
                        "description": (
                            "The context the AI should remember for the future interaction. "
                            "Describe what the AI should bring up when it contacts the user. "
                            "For example: 'Continue our discussion about science' or "
                            "'Ask how the user is progressing with the ESP32 project'."
                        ),
                    },
                },
                "required": [
                    "time",
                    "reason",
                ],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "changeVolume",
            "description": "Set the speaker volume to a value from 0 to 100.",
            "parameters": {
                "type": "object",
                "properties": {
                    "value": {
                        "type": "integer",
                        "description": "The volume percentage, from 0 to 100.",
                        "minimum": 0,
                        "maximum": 100
                    }
                },
                "required": ["value"]
            }
        }
    }
]

def build_messages(esp32_info, message):
    return [
        {
            "role": "system",
            "content": SYSTEM_PROMPT.format(esp32_info=esp32_info),
        },
        {
            "role": "user",
            "content": message,
        },
    ]


def build_wakeup_messages(esp32_info, reason):
    system = (
        SYSTEM_PROMPT.format(esp32_info=esp32_info)
        + PROACTIVE_ADDENDUM.format(reason=reason)
    )

    return [
        {
            "role": "system",
            "content": system,
        },
        {
            "role": "user",
            "content": (
                "Start the conversation now.\n"
                f"Topic: {reason}\n"
                "Say one natural sentence to the user about this topic."
            ),
        },
    ]
    
def format_device_info(arguments: dict) -> str:
    lines = []
    if "date_time" in arguments:
        try:
            t = datetime.strptime(arguments["date_time"], "%Y-%m-%d %H:%M:%S")
            lines.append(
                f"Current date : {WEEKDAYS[t.weekday()]} {t.day} {MONTH[t.month-1]} {t.year}, "
                f"Current time : {t.hour} h {t.minute:02d}"
            )
        except ValueError:
            lines.append(f"Current time : {arguments['date_time']}")
    if "timezone" in arguments:
        lines.append(f"Current timezone : {arguments['timezone']}")
    if "battery" in arguments:
        lines.append(f"Current device battery level : {arguments['battery']} percent")
    if "volume" in arguments:
        lines.append(f"Current device volume : {arguments['volume']} (value from 0 to 100)")
    if "location" in arguments:
        lines.append(f"Current device and user location : {arguments['location']}")
    return "\n".join(lines)

def get_tools() -> list[dict]:
    try:
        if isinstance(TOOLS_JSON, list):
            return TOOLS_JSON
        else:
            print("[AI CONTEXT UTILS] Tools JSON is not a list. Returning empty list.")
            return []
    except json.JSONDecodeError as e:
        print(f"[AI CONTEXT UTILS] Error decoding tools JSON: {e}. Returning empty list.")
        return []