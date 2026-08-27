def generate(client, model, question):

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
            if text.endswith(('.', '!', '?', ',', ';', ':')):
                print(sentence, end='', flush=True)
                yield sentence
                sentence = ""
    yield "\n"