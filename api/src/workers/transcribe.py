from services.redis_controller import RedisController
from services.transcriptor import Transcriptor
from config import (REDIS_KEY_PREFIX_TRANSCRIPTION, SIGNAL_TRANSCRIPTION_START, SIGNAL_TRANSCRIPTION_END, 
    REDIS_KEY_PREFIX_RECORD, SIGNAL_RECORDING_START, SIGNAL_RECORDING_END, TRANSCRIPTION_CHUNK_SIZE, #
    TRANSCRIPTION_WINDOW_SIZE, VOICE_LANGUAGE, REDIS_KEY_PREFIX_LANGUAGE)
import asyncio

_ASYNC_GENERATOR_END = object() # Object used to signal the end of the async generator

# Purpose: wrap sync blocking `transcriptor.transcribe` generator into an async generator to avoid blocking the event loop
async def async_generator_wrapper(sync_gen_func, *args, **kwargs):
    loop = asyncio.get_running_loop()
    out_queue: asyncio.Queue = asyncio.Queue()

    def runner():
        try:
            for item in sync_gen_func(*args, **kwargs):
                loop.call_soon_threadsafe(out_queue.put_nowait, item)
        except Exception as e:
            loop.call_soon_threadsafe(out_queue.put_nowait, e)
        finally:
            loop.call_soon_threadsafe(out_queue.put_nowait, _ASYNC_GENERATOR_END)

    loop.run_in_executor(None, runner)

    while True:
        item = await out_queue.get()
        if item is _ASYNC_GENERATOR_END:
            break
        if isinstance(item, Exception):
            raise item
        yield item

# purpose: process audio chunks and transcribe them into text
async def worker_transcribe(just_id: str, redis_controller : RedisController, transcriptor: Transcriptor): 
    remaining_audio = bytearray()
    segment_to_transcribe = b""
    new_segment_ready_for_transcription = False
    segment_counter = 0
    trans_key = REDIS_KEY_PREFIX_TRANSCRIPTION + just_id
    lang_key = REDIS_KEY_PREFIX_LANGUAGE + just_id
    print(f"[WORKER] Started: {trans_key}")
    
    temp_wav = b""
    
    while True:
        enough_audio_bytes_for_transcription = False
        new_segment_ready_for_transcription = False
        # Wait for a new audio chunk to be available in Redis
        audio_stream_id = REDIS_KEY_PREFIX_RECORD + just_id
        result = await redis_controller.blpop(audio_stream_id)
        
        if result is None:
            break
        else:
            _, audio = result
        
        if audio is None:
            break
                
        isAudioChunk = audio not in [bytes(SIGNAL_RECORDING_START, 'utf-8'), bytes(SIGNAL_RECORDING_END, 'utf-8')]
         
        if isAudioChunk and isinstance(audio, bytes):
            # print(f"[WORKER] Received audio chunk: {len(element)} bytes")
            temp_wav += audio
            remaining_audio.extend(audio)
            is_first_segment = segment_counter == 0
            enough_audio_bytes_for_transcription = len(remaining_audio) >= TRANSCRIPTION_CHUNK_SIZE
            
        if enough_audio_bytes_for_transcription:
            if is_first_segment: # for the first segment, no need to glide the window
                segment_to_transcribe = bytes(remaining_audio[:TRANSCRIPTION_CHUNK_SIZE])
                del remaining_audio[:TRANSCRIPTION_CHUNK_SIZE]
                new_segment_ready_for_transcription = True
            else:
                SPLITTER = TRANSCRIPTION_CHUNK_SIZE-TRANSCRIPTION_WINDOW_SIZE
                first_part = segment_to_transcribe[SPLITTER:]
                second_part = bytes(remaining_audio[:SPLITTER])
                segment_to_transcribe = (first_part + second_part)
                del remaining_audio[:SPLITTER]
                segment_to_transcribe = first_part + second_part
                new_segment_ready_for_transcription = True
        elif audio == bytes(SIGNAL_RECORDING_END, 'utf-8'):
            segment_to_transcribe = remaining_audio
            remaining_audio = bytearray()
            new_segment_ready_for_transcription = True
        
        if (new_segment_ready_for_transcription):
            segment_counter += 1
            lang = VOICE_LANGUAGE
            if lang == "auto":
                lang = None
                
            # --- seul changement : boucle sync remplacée par le wrapper async ---
            async for words, lang in async_generator_wrapper(
                transcriptor.transcribe, segment_to_transcribe, lang
            ):
                await redis_controller.r_push_expire(
                    lang_key, 
                    lang
                )
                await redis_controller.r_push_expire(
                    trans_key, 
                    f"{segment_counter}:{words}"
                )
                
        if audio == bytes(SIGNAL_RECORDING_START, 'utf-8'):
            await redis_controller.r_push_expire(trans_key, SIGNAL_TRANSCRIPTION_START)
            print(f"[WORKER] Received start signal for: {audio_stream_id}")
            continue  # Skip processing if the start signal is received
        
        if audio == bytes(SIGNAL_RECORDING_END, 'utf-8'):
            # segment_to_transcribe = processed_audio_bytes
            print(f"[WORKER] Received end signal for: {audio_stream_id}")
            await redis_controller.r_push_expire(trans_key, SIGNAL_TRANSCRIPTION_END)
            break  # Exit the loop if the end signal is received