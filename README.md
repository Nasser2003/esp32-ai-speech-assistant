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
- 

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

## 2. ollama
Ollama is a tool developped by Meta to make the ai management easier. It can be seen as the "Docker" of ia models.
You can Install it and run your first model:
```bash
ollama run llama3.2
```
This model is pretty fast on the code i5 CPU 

## 3. Flask API
To setup Flask, the command must be run `pip3 install flask`
To run the api: `python main.py`