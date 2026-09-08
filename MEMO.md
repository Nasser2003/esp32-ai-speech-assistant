
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

## Generate `.env.example` with one command

```bash
uv add dotenv-linter
uv sync
dotenv-linter --generate-example
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
