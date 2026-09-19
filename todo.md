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

## DOING

- learn about postgre and how to integrate it with the api

## TODO

- when waken up from timer, fetch planned task from api, then run the current one and remove from postgre
- create a postgre table for planned tasks (esp-id => mac)
- implement tasks in ESP32 (alarm)
- create a custom model with no md, context, override personality
- inject in the model the features it can have access to
- create a protocol so the esp understands which commands it should run

- wake from sleep after future planned alarm

- Improve pipeline with pipecat
- Transform application to systemctl start
- implement an architecture for context memory management in postgre
- create custom pcb + custom 3d model for the device
- Warning, if in the future, the model becomes faster, the esp32 could be saturated by audio bytes
- implement api authentication to secure connection
- expose my api in public, and make it more secure
