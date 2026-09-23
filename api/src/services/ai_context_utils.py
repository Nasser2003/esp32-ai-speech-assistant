import json
from datetime import datetime, timezone
from sqlalchemy.orm import Session
from models.ai_context import SystemContext, Message, ContextTypeEnum, RoleEnum

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

def get_or_create_system_context(
    db: Session,
    device: str | None = None,
    context_type: ContextTypeEnum = ContextTypeEnum.SYSTEM,
) -> SystemContext:
    query = db.query(SystemContext).filter(SystemContext.type == context_type)
    if device is not None:
        device_ctx = query.filter(SystemContext.device == device).first()
        if device_ctx:
            return device_ctx

    default_ctx = query.filter(SystemContext.device.is_(None)).first()
    if default_ctx:
        return default_ctx

    # Fallback: create base system context if none exists in DB
    new_ctx = SystemContext(
        type=context_type,
        content=SYSTEM_PROMPT,
        device=None,
    )
    db.add(new_ctx)
    db.commit()
    db.refresh(new_ctx)
    return new_ctx


def get_recent_messages(
    db: Session,
    system_id: int,
    limit: int = 10,
    device: str | None = None,
) -> list[Message]:
    query = db.query(Message).filter(Message.system == system_id)
    if device is not None:
        query = query.filter((Message.device == device) | (Message.device.is_(None)))
    messages = (
        query.order_by(Message.created_at.desc(), Message.id.desc())
        .limit(limit)
        .all()
    )
    messages.reverse()
    return messages


def save_message(
    db: Session,
    system_id: int,
    role: RoleEnum,
    content: str,
    device: str | None = None,
) -> Message | None:
    if not content or not content.strip():
        return None
    msg = Message(
        system=system_id,
        role=role,
        content=content.strip(),
        device=device,
        created_at=datetime.now(timezone.utc),
    )
    db.add(msg)
    db.commit()
    db.refresh(msg)
    return msg


def get_system_summary(db: Session, device: str | None = None) -> str | None:
    query = db.query(SystemContext).filter(SystemContext.type == ContextTypeEnum.SUMMARY)
    if device is not None:
        dev_summary = query.filter(SystemContext.device == device).first()
        if dev_summary and dev_summary.content:
            return dev_summary.content
    summary = query.filter(SystemContext.device.is_(None)).first()
    return summary.content if summary and summary.content else None


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


def build_messages_with_context(
    db: Session,
    esp32_info: str,
    message: str,
    limit: int = 10,
    device: str | None = None,
) -> tuple[list[dict], int]:
    sys_ctx = get_or_create_system_context(db, device=device)
    base_prompt = sys_ctx.content

    if "{esp32_info}" in base_prompt:
        system_content = base_prompt.format(esp32_info=esp32_info)
    else:
        system_content = f"{base_prompt}\n\nDevice reference:\n{esp32_info}"

    messages: list[dict] = [
        {
            "role": "system",
            "content": system_content,
        }
    ]

    summary = get_system_summary(db, device=device)
    if summary:
        messages.append(
            {
                "role": "system",
                "content": f"Summary of previous conversation:\n{summary}",
            }
        )

    recent_messages = get_recent_messages(db, system_id=sys_ctx.id, limit=limit, device=device)
    for hist_msg in recent_messages:
        messages.append(
            {
                "role": hist_msg.role.value if hasattr(hist_msg.role, "value") else str(hist_msg.role),
                "content": hist_msg.content,
            }
        )

    messages.append(
        {
            "role": "user",
            "content": message,
        }
    )

    return messages, sys_ctx.id


def build_wakeup_messages_with_context(
    db: Session,
    esp32_info: str,
    reason: str,
    limit: int = 10,
    device: str | None = None,
) -> tuple[list[dict], int]:
    sys_ctx = get_or_create_system_context(db, device=device)
    base_prompt = sys_ctx.content

    if "{esp32_info}" in base_prompt:
        system_content = base_prompt.format(esp32_info=esp32_info)
    else:
        system_content = f"{base_prompt}\n\nDevice reference:\n{esp32_info}"

    sub_ctx = db.query(SystemContext).filter(SystemContext.type == ContextTypeEnum.SUB_SYSTEM).first()
    if sub_ctx and "{reason}" in sub_ctx.content:
        addendum = sub_ctx.content.format(reason=reason)
    elif sub_ctx:
        addendum = f"\n\n{sub_ctx.content}\nReason to bring this up: {reason}"
    else:
        addendum = PROACTIVE_ADDENDUM.format(reason=reason)

    system_content += addendum

    messages: list[dict] = [
        {
            "role": "system",
            "content": system_content,
        }
    ]

    summary = get_system_summary(db, device=device)
    if summary:
        messages.append(
            {
                "role": "system",
                "content": f"Summary of previous conversation:\n{summary}",
            }
        )

    recent_messages = get_recent_messages(db, system_id=sys_ctx.id, limit=limit, device=device)
    for hist_msg in recent_messages:
        messages.append(
            {
                "role": hist_msg.role.value if hasattr(hist_msg.role, "value") else str(hist_msg.role),
                "content": hist_msg.content,
            }
        )

    messages.append(
        {
            "role": "user",
            "content": (
                "Start the conversation now.\n"
                f"Topic: {reason}\n"
                "Say one natural sentence to the user about this topic."
            ),
        }
    )

    return messages, sys_ctx.id

    
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