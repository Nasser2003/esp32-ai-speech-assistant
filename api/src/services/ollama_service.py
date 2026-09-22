from ollama import AsyncClient
from services.ai_context_utils import build_messages

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
    