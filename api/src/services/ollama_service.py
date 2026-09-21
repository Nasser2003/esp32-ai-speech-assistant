from ollama import AsyncClient

SYSTEM_PROMPT = """You are a voice assistant. The user speaks aloud into the microphone of a small ESP32 device. Their speech is transcribed into text for you (the transcription may contain errors). Your response is then read aloud through the device’s speaker.

The device is your means of communication, not the subject of the conversation. Only mention the device if the user asks a question about it.

Rules, since your responses are spoken aloud:
- Always respond in French.
- Keep sentences short—generally one to three sentences. Get straight to the point.
- No Markdown, no lists, no headings, no emojis, no code, no URLs.
- Write numbers, units, and times as they are spoken (“twenty-two degrees,” “three:30 p.m.”).
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

async def ask_ai(client: AsyncClient, model: str, question: dict, esp32_info: str = ""):
    # Ask the AI a question
    response = await client.chat(
        model=model,
        keep_alive="10m",
        options={
            "temperature": 0.2, # more coherent and less creative responses
            "top_p": 0.9,
            "num_ctx": 8192, # context window size
            "num_predict": 300, # maximum number of tokens to predict
        },
        messages=build_messages(esp32_info, question),
        stream=True
    )

    print(build_messages(esp32_info, question))

    sentence = ""

    async for chunk in response:
        text = chunk["message"]["content"]

        if text:
            sentence += text
            if text.strip().endswith(('.', '!', '?', ',', ';', ':', '\n')) and len(sentence) > 10:
                yield sentence
                sentence = ""
    yield f"{sentence}\n"
    