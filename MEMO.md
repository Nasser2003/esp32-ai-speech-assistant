
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