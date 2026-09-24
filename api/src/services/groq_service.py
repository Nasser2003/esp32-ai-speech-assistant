import json
import traceback
from collections.abc import AsyncGenerator

from groq import AsyncGroq

from services.ai_context_utils import TOOLS_JSON
from services.ai_tools import AiToolsManager


async def ask_ai(
    client: AsyncGroq,
    model: str,
    messages: list[dict],
    tool_manager: AiToolsManager,
) -> AsyncGenerator[str, None]:

    try:
        for iteration in range(2):

            print(f"[AI ASK] LLM iteration {iteration + 1}")

            response = await client.chat.completions.create(
                model=model,
                messages=messages,
                tools=TOOLS_JSON,
                tool_choice="auto",
                temperature=0.2,
                top_p=0.9,
                max_tokens=1000,
                stream=True,
            )

            print("[AI ASK] Context: ", messages)

            sentence = ""
            full_content = ""
            # index -> {"id": str, "name": str, "arguments": str}
            tool_calls_acc: dict[int, dict] = {}

            async for chunk in response:

                if not chunk.choices:
                    continue

                delta = chunk.choices[0].delta

                # --------------------------------------------------
                # AI RESPONSE
                # --------------------------------------------------

                if delta.content:
                    text = delta.content
                    full_content += text
                    sentence += text

                    # Ne déclenche la coupe par nombre de mots que si on est bien
                    # entre deux mots, jamais en plein milieu d'un token de mot.
                    at_word_boundary = sentence.endswith(
                        (" ", ".", "!", "?", ",", ";", ":", "\n")
                    )

                    if (
                        text.strip().endswith((".", "!", "?", ",", ";", ":", "\n"))
                        and len(sentence) > 10
                    ):
                        yield sentence
                        sentence = ""
                    elif at_word_boundary and len(sentence.split()) >= 3:
                        words = sentence.split()
                        yield " ".join(words[:5]) + " "
                        sentence = " ".join(words[5:])

                # --------------------------------------------------
                # AI TOOL CALLS
                # --------------------------------------------------
                # Groq (comme OpenAI) envoie les tool calls fragmentés sur
                # plusieurs chunks : id/name arrivent au premier chunk de
                # l'appel, "arguments" arrive ensuite morceau par morceau.
                # On accumule par index jusqu'à la fin du stream.

                if delta.tool_calls:
                    for tc_delta in delta.tool_calls:

                        entry = tool_calls_acc.setdefault(
                            tc_delta.index,
                            {"id": None, "name": None, "arguments": ""},
                        )

                        if tc_delta.id:
                            entry["id"] = tc_delta.id

                        if tc_delta.function and tc_delta.function.name:
                            entry["name"] = tc_delta.function.name

                        if tc_delta.function and tc_delta.function.arguments:
                            entry["arguments"] += tc_delta.function.arguments

            tool_calls = list(tool_calls_acc.values())

            for tool_call in tool_calls:
                print(
                    f"[AI TOOLS] Tool call detected: {tool_call['name']} "
                    f"with arguments: {tool_call['arguments']}"
                )

            # ------------------------------------------------------
            # NO TOOL CALL
            # ------------------------------------------------------

            if not tool_calls:

                if sentence:
                    yield sentence

                break

            # ------------------------------------------------------
            # TOOL CALL FOUND
            # ------------------------------------------------------

            # On ne veut pas faire une troisième requête.
            if iteration == 1:
                print(
                    "[AI TOOLS] Tool call returned on final iteration, "
                    "stopping."
                )
                break

            # Ajouter la réponse du modèle contenant les tool calls.
            # id + type "function" sont obligatoires pour que Groq accepte
            # ce message dans le tour suivant.
            messages.append(
                {
                    "role": "assistant",
                    "content": full_content,
                    "tool_calls": [
                        {
                            "id": tool_call["id"],
                            "type": "function",
                            "function": {
                                "name": tool_call["name"],
                                "arguments": tool_call["arguments"],
                            },
                        }
                        for tool_call in tool_calls
                    ],
                }
            )

            # ------------------------------------------------------
            # EXECUTE TOOLS
            # ------------------------------------------------------

            for tool_call in tool_calls:

                try:
                    args = json.loads(tool_call["arguments"] or "{}")
                except json.JSONDecodeError:
                    print(
                        f"[AI TOOLS] Invalid JSON arguments for "
                        f"{tool_call['name']}: {tool_call['arguments']!r}"
                    )
                    args = {}

                result = await tool_manager.dispatch_tool(
                    tool_call["name"],
                    **args,
                )

                print(
                    f"[AI TOOLS] Tool result: {result}"
                )

                # tool_call_id relie ce résultat à l'appel précis que le
                # modèle a fait, indispensable dès qu'il y a plusieurs
                # appels en parallèle.
                messages.append(
                    {
                        "role": "tool",
                        "tool_call_id": tool_call["id"],
                        "content": str(result),
                    }
                )

            # La boucle revient au début.
            # Le LLM reçoit maintenant :
            #
            # user
            # assistant -> tool call
            # tool -> résultat
            #
            # et peut générer sa réponse finale.

        if sentence:
            yield f"{sentence}\n"

    except Exception as e:
        print(
            f"[AI ASK] Exception occurred while streaming AI response: {e}"
        )
        traceback.print_exc()
        raise