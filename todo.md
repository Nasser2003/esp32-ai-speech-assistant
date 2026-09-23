# Features suggestion

## DONE

- prepare the migration from ESP32 S3 to ESP32 C3 super mini
- Use one I2S instead of 2, to make the project compatible with ESP32 C3 super mini
- dockerize the project (api, postgres, redis)
- introduce battery level gpio to measure the battery
- implement BLE provisionning to dynamically connect esp32 to Wifi
- deep sleep after long idle(30s)
- wake from sleep after button click
- wake up from sleep every minute to check for new tasks
- when waken up from button, speedup the init
- learn about postgre and how to integrate it with the api
- create a postgre table for planned tasks (esp-id => mac)
- when waken up from timer, fetch planned task from api, then run the current one and change status
- implement in the esp32, service api calls to know the current type and location of the device
- replace `lang_detector` python library with `fasttext-numpy2` for more accuracy in text language prediction
- implement tasks in ESP32 (alarm, change volume, message from ai) with its arguments

## DOING

- inject in the model the tools it can have access to

## TODO

### task execution

- give ESP32 context (battery, time(from api), volume)
- find a way to make the model execute some tasks (produce special commands and intercept by the API)
- learn freeCAD to create a design for the device

### memory management

- implement an architecture for context memory management in postgre

### device shape

- create a custom model with no md, context, override personality
- use a generic pcb to create a more compact device

### for later, public access

- improve battery accuracy (0%->10%, 100% -> 90%)
- Improve pipeline with pipecat
- Transform application to systemctl start
- Warning, if in the future, the model becomes faster, the esp32 could be saturated by audio bytes
- implement api authentication to secure connection
- expose my api in public, and make it more secure
