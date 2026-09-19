
# Personal notes

## Install a python dependencie and update the project

```bash
# 1. Add the dependancie in the project configuration
uv add <dep>
# 2. Or install in the host project
uv sync
# 3. Install inside a container to refrech the running server
podman compose exec api uv sync
```

## Check `.env` Synthax

```bash
uv add dotenv-linter
uv sync
dotenv-linter --generate-example
```

## Generate `.env.example` with a script

```bash
scripts/env-example-generator.sh api/.env
```

## Podman compose

### Only build

```bash
# Lines from Dockerfile are cached by default if the content of these lines hasn't changed (identic hash)
# But if one line changes, then all the next lines won't be cached and will be executed again (cascade)
podman compose -f docker-compose.yml build 
# Fully builds the image without using cache, without skipping lines with FROM, ENV, WORKDIR, COPY, ADD
podman compose -f docker-compose.yml build --no-cache
```

### Run podman compose

```bash
# Runs services contained in `docker-compose.yml` such as images
podman compose -f docker-compose.yml up 
# Runs the services in the background, detached from cli
podman compose -f docker-compose.yml up -d
# Builds and runs the services
podman compose -f docker-compose.yml up --build
# Builds with no cache and runs the services
podman compose -f docker-compose.yml up --build --no-cache
```

### Access logs when detached cli

```bash
podman compose -f logs
```

## Redis

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

## ESP32 saving mode

### Wifi sleep mode

Instead of turning off the wifi, the sleep mode could be useful to reduce the power, and turn on directly when needed.

```cpp
#include <WiFi.h>

WiFi.setSleep(true)
// WiFi sleep
WiFi.setSleep(false)
```

### Light Sleep mode

The consumption of this mode is arround 600 times lower than the active mode.
This is the light way to save energy, it freezes the CPU, while keeping the ram and registers.
To use this mode, we need to define the wakeup condition (gpio activation, timer, or both)

```cpp
#include "esp_sleep.h"

// wake up conditions
//  Timer activation
esp_sleep_enable_timer_wakeup((uint64_t) SLEEP_SECS * 1000000ULL)

//  GPIO activation
gpio_wakeup_enable((gpio_num_t)BUTTON_PIN, GPIO_INTR_HIGH_LEVEL);

// Flush Serial buffer
Serial.flush();

// Starting the light sleep mode
esp_light_sleep_start();
```

### Deep Sleep mode

The consumption of this mode is arround 16 000 times lower than the active mode.
Almost everything is powered off (CPU, RAM, GPIO, Register) except for RTC module and RTC memory.
When ESP32 is waken up from deep sleep mode, it reboots from setup().
Be aware that for the GPIO activation, only GPIOs having the RTC feature can be used.

Since in this mode, all the variables stored in ram are lost, there is a special memory that can be used:
RTC memory. On esp32 c3 super mini, 8 000 bytes are available. But they are lost if power off or ESP reset.

```cpp
#include "esp_sleep.h"

// Create variables stored in RTC Memory (via macro)
RTC_DATA_ATTR int bootCount = 0;

// We can identify the event that woke up the esp from deep sleep
esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

switch (cause) {
    case ESP_SLEEP_WAKEUP_TIMER:
        Serial.println("Wakeup cause: RTC Timer");
        break;
    case ESP_SLEEP_WAKEUP_GPIO:
        Serial.println("Wakeup ause: GPIO");
        break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
        Serial.println("Wakeup cause: Touch sensor");
        break;
    default: // ESP_SLEEP_WAKEUP_UNDEFINED
        Serial.println("Wakeup cause: Power-on / hard reset");
        break;
}

bootCount++;
Serial.print("Boot: ");
Serial.println(bootCount);

// Set timer deep sleep condition
esp_sleep_enable_timer_wakeup((uint64_t) SLEEP_SECS * 1000000ULL);

// Set gpio deep sleep condition
esp_deep_sleep_enable_gpio_wakeup(
    (1ULL << wakeUpPin),
    ESP_GPIO_WAKEUP_GPIO_LOW
);

Serial.flush();

// start deep sleep
esp_deep_sleep_start();
```

### Hibernation mode (advanced deep sleep)

This mode is more restricted than the deep sleep mode because it can be only waken up through RTC pin trigger.
It does not keep RTC memory.

```cpp
#include "esp_sleep.h"

#define WAKEUP_PIN GPIO_NUM_5

void enterHibernation()
{
    // Wake up when GPIO5 becomes LOW.
    esp_deep_sleep_enable_gpio_wakeup(
        1ULL << WAKEUP_PIN,
        ESP_GPIO_WAKEUP_GPIO_LOW
    );

    // Turn off RTC FAST memory.
    // This means RTC_DATA_ATTR variables will NOT survive.
    esp_sleep_pd_config(
        ESP_PD_DOMAIN_RTC_FAST_MEM,
        ESP_PD_OPTION_OFF
    );

    // Enter the lowest-power deep-sleep configuration possible.
    Serial.flush();
    esp_deep_sleep_start();
}
```

## PostgreSQL

### Connect to postgre via the container

```bash
podman compose exec postgres bash
psql -U admin -d ai_context
```

### Connect to postgre via pgAdmin container

Go to the link: `http://<server-ip>:8080/browser/` and create a connection to the postgres database.

### RUN Commands in psql

```bash
# list all databases
\l
# list all tables
\dt
# clear terminal
\! clear
# select database
```

### Basic commands

WARNING: If you enter commands via the terminal, do not forget to put `;` semicolon at the end in order to be executed.

```sql
CREATE TABLE IF NOT EXISTS people (
    id SERIAL PRIMARY KEY,
    first_name VARCHAR(255) NOT NULL,
    last_name VARCHAR(255),
    age INT CHECK(age > 0),
    job VARCHAR(255),
    employed BOOL
);

INSERT INTO people (first_name, last_name, age, job, employed) VALUES 
    ('Mike', 'Smith', 30, 'Programmer', true),
    ('Mike2', 'Smith2', 31, 'Doctor', false),
    ('Mike3', 'Smith3', 32, 'Teacher', true);

SELECT * FROM people;
```
