from ollama import AsyncClient


async def ask_ai(client: AsyncClient, model: str, question: str):

    response = await client.chat(
        model=model,
        messages=[
            {"role": "user", "content": question}
        ],
        stream=True
    )

    sentence = ""

    async for chunk in response:
        text = chunk["message"]["content"]

        if text:
            sentence += text
            if text.strip().endswith(('.', '!', '?', ',', ';', ':', '\n')) and len(sentence) > 10:
                yield sentence
                sentence = ""
    yield f"{sentence}\n"
    