import asyncio
from fastapi import WebSocket

async def terminate_session_if_workers_done(worker_tasks, client_ws: WebSocket, just_id: str):
    results = await asyncio.gather(*worker_tasks, return_exceptions=True)
    for index, result in enumerate(results, start=1):
        if isinstance(result, Exception):
            print(f"[WS] Worker {index} failed: {result}", flush=True)

    print(f"[WS] All workers finished for {just_id}", flush=True)
    await client_ws.close()