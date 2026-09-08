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
- Use redis database to store in memory data like sessions and audio
- Use caching with Flask
- Create async tasks with asyncio
- Transcribe audio using Whisper from OpenAi
- Convert text into Speech with Piper

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

## 3. Fast API
>
> **Flask** \
Previously, i used Flask API, a quick python library to build APIs very fast, before migrating to Fast API.

Fast api is a modern, fast, asynchronous, and scallable API compared to Flask.
By migrating to Fast API, the code became much easier and simplier:

- Async tasks: native, no need to create threads
- Web sockets: natively supported, no need to install extra libraries
- Open api, docs, and redoc natively implemented

### Create a python environment

```bash
sudo apt install python3-venv
python3 -m venv .venv
pip3 install uv
source .venv/bin/activate
uv add --requirements requirements.txt
```

To setup FastAPI, the command must be run `

To start the installation

```bash

uv sync
podman-compose -f ../docker-compose.yml build --no-cache
```

To run the api:

```bash
podman-compose -f ../docker-compose.yml up -d
```

> Note: the 0.0.0.0 allows the api to be accessed outside of localhost
To create a web socket route:

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

See [memo](MEMO.md)

## 5. Whisper

Whisper is a technologie that transcribes audio into text by using ai models.
There are different types (tiny, small, medium, large...), They accept audio files, buffers
and returns text.

## 6. Piper TTS

It's an open source project to transform text into voice. you can download voices for example:

```bash
python3 -m piper.download_voices en_US-ryan-low fr_FR-gilles-low
```

## 7. PostGreSQL

A powerful relational database I will use to store the context (chat conversation).

## 8. Docker/Podman

Docker is a containerization platform used to run our services into isolated containers.
This is useful to run our services separately, for example to:

- Prevent from dependencies conflict
- Control the network of each service
- Reduce the impact when a service crashes
- Run multiple instances of one service to ensure availability

With Docker, we will containerize 3 services:

- FlastAPI
- Redis database
- PostGre SQL database

But instead of using Docker like usual, we will use another similar alternative called Podman.

### Podman - rootless Docker

Podman is a containerization platform like Docker, except that it doesn't use a Daemon that requires root permission. With Podman, we can run commands without using sudo which reduces the risk of executing malicious containers at root level.

### Pdoman Installation

```bash
sudo apt update
sudo apt install -y podman
pip install podman-compose
# since podman is rootless, we should give him a range of user id to create virtual users in the containers
sudo usermod --add-subuids 100000-165535 --add-subgids 100000-165535 $USER
```

### Set Up

Once Podman is installed on the server, we can run the following command:

```bash
docker-compose up -d 
```
