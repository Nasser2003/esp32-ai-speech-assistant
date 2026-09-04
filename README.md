# Project synthese
In this document, I will describe each step of developping this project in order to not forget how I did it.
## Topics I will learn in this project
- Use advanced ESP32 features (flash storage, I2C communication, mic, sound, oled screen)
- Manage complex state machine on ESP32
- Deploy my first ai assistant with Ollama and use it in the project
- Deploy a model that transcripts audio received from the esp32 mic.
- Learn C++ and make modular code
- Create web sockets on ESP
- Stream full-duplex audio and text information between ESP32 and API
- How to use redis database to store in memory data like sessions and audio
- How to use caching with Flask
- How to create async tasks with asyncio
- How to transcribe audio using Whisper from OpenAi

## 1. ESP32
### PlatformIO
I use the plugin "PlatformIO" which plays the same role as ArduinoIDE. We can create project, load it, download libraries, run serial monitoring,...

The configuration of the ESP is in the [platformio.ini](../platformio.ini). It depends on the model.

We have some basic commands
```bash
# Upload our code
pio run -t upload
# Upload files in "/data"
pio run -t uploadfs
# Access the logs of our esp
pio device monitor
```
Remark: you need to add the **pio path** to the **environment** to use the `"pio"` applet. 
Otherwise, we would need to use `"%USERPROFILE%\.platformio\penv\Scripts\platformio.exe"`

## 2. Ollama
Ollama is a tool developped by Meta to make the ai management easier. It can be seen as the "Docker" of ia models.
### Installation and run
You can Install it and run your first model:
```bash
# 1. install
curl -fsSL https://ollama.com/install.sh | sh
# 2. run
ollama run llama3.2
```
This model is pretty fast on the code i5 CPU 

## 3. Flask API
Flask API is a strong python library to build reliable APIs, 
and can be integrated with others tools easily.

### Create a python environment
```bash
sudo apt install python3-venv
python3 -m venv .venv
source .venv/bin/activate
pip3 install -r requirements.txt
```
### Use caching with Flask
The idea of caching is to store frequently accessed data in cache to save some flask performance
```bash
# in this example, this response of /hello will be saved 1 hour
@app.route("/hello")
@cache.cached(timeout = 3600)
def hello():
    return "hello"
```

To setup Flask, the command must be run `pip3 install flask`
To run the api: `python main.py`
To open it outside localhost: 
```python
if __name__ == '__main__':
    app.run(debug=True, host="0.0.0.0", port=5000)
```
To create a web socket route:
```python
# 1. Install extension of Flask for Web sockets
pip3 install flask-sock

# 2. Import in the main.py
from flask_sock import Sock

# 3. Define a route
@sock.route("/ws")
def websocket(ws):
    while True:
        message = ws.receive()
        if message is None:
            break
        if isinstance(message, bytes):
            ...
```

## 4. Redis
Redis is a NoSQL in memory database which is extremely fast with its very low latency performance.
It will be used to store audio data and sessions.
### Installation
```bash
sudo apt-get install redis
```
### Running and access
```bash
# To run
redis-server
# To stop
sudo systemctl stop redis-server
# To access
redis-cli
    # 127.0.0.1:6379>
```
### Usage
```bash
# 1. Set a key value
SET NAME JOHN
# 2. Get the value of the key NAME
GET NAME # JOHN
# 3. Delete a key
DEL NAME
GET NAME # (nil) -> means null
# 4. Check existance
SET NAME MARIA
EXISTS NAME # 1
EXISTS name # 0 -> case sensitive !!!
# 5. Get all keys
SET NAME2 MARC
KEYS *
    # NAME
    # NAME2
# 6. Delete all
FLUSHALL
# 7. See and set ttl so keys expire automatically
ttl NAME
    # -1 -> no expiration set
expire NAME 10 #-> expires after 10 secodes
    # after 5 seconds:
    ttl NAME
        # 5 -> five seconds left
    # after expiration:
    ttl NAME
        # -2 -> this key NAME doesn't exist anymore
# 7b. Combine SET and EXPIRE in one command
SETEX NAME3 10 KALE

# 8a. Create a list, add items, delete and show
LPUSH FRIENDS john mark maria # Push 3 values from the left inside Friends list
LRANGE friends 0 -1 # -> from 0 to the last item
    # 1) "maria"
    # 2) "mark"
    # 3) "john"
RPUSH FRIENDS peter # Push from the right
LRANGE friends 0 -1 # -> from 0 to the last item
    # 1) "maria"
    # 2) "mark"
    # 3) "john"
    # 4) "peter" -> pushed at the end
LPOP FRIENDS # -> pop first element
RPOP FRIENDS # -> pop last element

# 9. Create a set, add items, delete, and show
SADD "The days" monday monday tuesday wednesday
    # -> if our string is composed of words, we can wrap it into commas
    # -> Since Set is unique, it won't add dupplicated values
SMEMBERS "The days" # show the set content
# 1) "monday"
# 2) "wednesday"
# 3) "tuesday"
SREM "The days" wednesday # delete an element

# 10. Create a key value set, add items, delete, and show
HSET person name lucie
HGET person name
HSET person age 26
HGETALL person
HDEL person age
HEXISTS person name

# 11. We cannot use the wrong operation on a key, unless it's empty
lpush cars volvo peugeot
SADD cars citroen
    # => (error) WRONGTYPE Operation against a key holding the wrong kind of value
DEL cars
SADD cars citroen
    # Will work because the key is empty

# 12. Special case for list: blocking pop => waiting until new element is added
BRPOP orders 0 # -> 0 means wait permanently, even if list deleted!
```

## 5. Whisper
Whisper is a technologie that transcribes audio into text by using ai models.
There are different types (tiny, small, medium, large...), They accept audio files, buffers
and returns text.

## 6. Piper TTS
It's an open source project to transform text into voice. you can download voices for example:
```bash
python3 -m piper.download_voices en_US-ryan-low fr_FR-gilles-low
```
