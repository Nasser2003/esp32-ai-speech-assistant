import traceback
from collections.abc import AsyncGenerator

from ollama import AsyncClient

from services.ai_context_utils import TOOLS_JSON
from services.ai_tools import AiToolsManager


async def ask_ai(
    client: AsyncClient,
    model: str,
    messages: list[dict],
    tool_manager: AiToolsManager,
) -> AsyncGenerator[str, None]:

    try:
        for iteration in range(2):

            print(f"[AI ASK] LLM iteration {iteration + 1}")

            response = await client.chat(
                model=model,
                keep_alive="10m",
                options={
                    "temperature": 0.2,
                    "top_p": 0.9,
                    "num_ctx": 8192,
                    "num_predict": 300,
                },
                messages=messages,
                stream=True,
                tools=TOOLS_JSON,
            )

            sentence = ""
            tool_calls = []
            full_content = ""

            async for chunk in response:

                message = chunk.get("message", {})

                # --------------------------------------------------
                # AI RESPONSE
                # --------------------------------------------------

                text = message.get("content", "")

                if text:
                    full_content += text
                    sentence += text

                    if (
                        text.strip().endswith(
                            (".", "!", "?", ",", ";", ":", "\n")
                        )
                        and len(sentence) > 10
                    ):
                        yield sentence
                        sentence = ""

                # --------------------------------------------------
                # AI TOOL CALLS
                # --------------------------------------------------

                chunk_tool_calls = message.get("tool_calls", [])

                if chunk_tool_calls:
                    for tool_call in chunk_tool_calls:

                        print(
                            f"[AI TOOLS] Raw tool call: {tool_call}"
                        )

                        tool_name = tool_call.function.name
                        tool_args = tool_call.function.arguments

                        print(
                            f"[AI TOOLS] Tool call detected: "
                            f"{tool_name} with arguments: {tool_args}"
                        )

                        tool_calls.append(
                            {
                                "name": tool_name,
                                "arguments": tool_args,
                            }
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
            messages.append(
                {
                    "role": "assistant",
                    "content": full_content,
                    "tool_calls": [
                        {
                            "function": {
                                "name": tool_call["name"],
                                "arguments": tool_call["arguments"],
                            }
                        }
                        for tool_call in tool_calls
                    ],
                }
            )

            # ------------------------------------------------------
            # EXECUTE TOOLS
            # ------------------------------------------------------

            for tool_call in tool_calls:

                result = await tool_manager.dispatch_tool(
                    tool_call["name"],
                    **tool_call["arguments"],
                )

                print(
                    f"[AI TOOLS] Tool result: {result}"
                )

                messages.append(
                    {
                        "role": "tool",
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