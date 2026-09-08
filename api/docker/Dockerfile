# Use a stable version of Python (3.12 or 3.13)
FROM python:3.12-slim

# Set the working directory (all subsequent commands run from here)
WORKDIR /app

# 1. Copy configuration files first (Optimizes Docker layer caching)
COPY pyproject.toml uv.lock .env ./

# Install uv and install Python dependencies globally in the container
RUN pip install --no-cache-dir uv
RUN uv sync --locked --no-dev

# 2. Copy code and data directories to the correct relative paths (./src and ./data)
COPY src/ ./src/
COPY data/ ./data/

# Documentary line, has no effect on its own
EXPOSE 5000 

# This line will be overridden by your docker-compose.yml command
CMD ["uv", "run", "uvicorn", "src.main:app", "--host", "0.0.0.0", "--port", "5000"]
