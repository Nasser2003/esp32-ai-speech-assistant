from src.config import (PREFIX_TRANSCRIPTION, TRANSCRIPTION_START, TRANSCRIPTION_END, 
    TTL_EXPIRE_TIME, PREFIX_RECORD, RECORDING_START, RECORDING_END, TRANSCRIPTION_CHUNK_SIZE, #
    TRANSCRIPTION_WINDOW_SIZE, VOICE_LANGUAGE, PREFIX_LANGUAGE)

async def worker_transcribe(just_id, redis_controller, transcriptor): 
    remaining_audio = bytearray()
    segment_to_transcribe = b""
    new_segment_ready_for_transcription = False
    segment_counter = 0
    trans_key = PREFIX_TRANSCRIPTION + just_id
    lang_key = PREFIX_LANGUAGE + just_id
    print(f"[WORKER] Started: {trans_key}")
    
    temp_wav = b""
    
    while True:
        enough_audio_bytes_for_transcription = False
        new_segment_ready_for_transcription = False
        # Wait for a new audio chunk to be available in Redis
        audio_stream_id = PREFIX_RECORD + just_id
        _, element = redis_controller.blpop(audio_stream_id, TTL_EXPIRE_TIME)
        print(f"[WORKER] Received transcription element: {len(element)} bytes")
        IS_RECORDING_START = element == bytes(RECORDING_START, 'utf-8')
        IS_RECORDING_END = element == bytes(RECORDING_END, 'utf-8')
        isAudioChunk = not IS_RECORDING_END and not IS_RECORDING_START
         
        if element is None:
            continue
        elif isAudioChunk:
            # print(f"[WORKER] Received audio chunk: {len(element)} bytes")
            temp_wav += element
            remaining_audio.extend(element)
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
        elif IS_RECORDING_END:
            segment_to_transcribe = remaining_audio
            remaining_audio = bytearray()
            new_segment_ready_for_transcription = True
        
        if (new_segment_ready_for_transcription):
            segment_counter += 1
            lang = VOICE_LANGUAGE
            if lang == "auto":
                lang = None
            for words, lang in transcriptor.transcribe(segment_to_transcribe, lang):
                redis_controller.r_push_expire(
                    lang_key, 
                    lang, 
                    TTL_EXPIRE_TIME
                )
                redis_controller.r_push_expire(
                    trans_key, 
                    f"{segment_counter}:{words}",
                    TTL_EXPIRE_TIME
                )
                
        if IS_RECORDING_START:
            redis_controller.r_push_expire(trans_key, TRANSCRIPTION_START, TTL_EXPIRE_TIME)
            print(f"[WORKER] Received start signal for: {audio_stream_id}")
            continue  # Skip processing if the start signal is received
        
        if IS_RECORDING_END:
            # segment_to_transcribe = processed_audio_bytes
            print(f"[WORKER] Received end signal for: {audio_stream_id}")
            redis_controller.r_push_expire(trans_key, TRANSCRIPTION_END, TTL_EXPIRE_TIME)
            break  # Exit the loop if the end signal is received