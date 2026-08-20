# ── Stage 1: Build ──
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ gcc cmake make ca-certificates curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY CMakeLists.txt ./
COPY src/ ./src/

RUN mkdir build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && make -j$(nproc)

# ── Stage 2: Runtime ──
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    docker.io ca-certificates curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy binary from builder (sqlite3 is statically linked)
COPY --from=builder /app/build/online_compiler .

# Copy frontend assets
COPY public/ ./public/

# Create data directory for SQLite
RUN mkdir -p /app/data

# Default environment
ENV PORT=3000
ENV DOCKER_TIMEOUT_SECONDS=10
ENV DOCKER_MEMORY_LIMIT=128m
ENV DOCKER_CPU_LIMIT=0.5
ENV DOCKER_PIDS_LIMIT=50
ENV RATE_LIMIT_MAX_REQUESTS=10
ENV RATE_LIMIT_WINDOW_SECONDS=60
ENV PUBLIC_DIR=/app/public

EXPOSE 3000

CMD ["./online_compiler"]
