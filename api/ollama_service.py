def ask_ai(client, model, question):

    response = client.chat(
        model=model,
        messages=[
            {"role": "user", "content": question}
        ],
        stream=True
    )

    sentence = ""

    for chunk in response:
        text = chunk["message"]["content"]

        if text:
            sentence += text
            if text.strip().endswith(('.', '!', '?', ',', ';', ':')) and len(sentence) > 10:
                yield sentence
                sentence = ""
    yield f"{sentence}\n"
    