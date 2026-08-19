# Deployment Guide

## Local Development

### Prerequisites

- macOS or Linux (Windows: use WSL2)
- C++17 compiler (g++ 9+ or clang++ 10+)
- CMake 3.14+
- Docker installed and running
- Current user in `docker` group (no sudo needed)

### Build

```bash
cd app
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)  # macOS: make -j$(sysctl -n hw.ncpu)
```

**Output**: `app/build/online_compiler` (executable)

### Run

```bash
cd app/build
./online_compiler
```

**Output:**
```
(2026-07-26 10:00:00) [INFO] Crow/1.0 server is running at http://0.0.0.0:3000
Database initialized (data/submissions.db)
```

Open **http://localhost:3000** in browser.

### Configuration

Set environment variables before running:

```bash
export PORT=3000
export DOCKER_TIMEOUT_SECONDS=10
export DOCKER_MEMORY_LIMIT=128m
export DOCKER_CPU_LIMIT=0.5
export DOCKER_PIDS_LIMIT=50
export RATE_LIMIT_WINDOW_SECONDS=60
export RATE_LIMIT_MAX_REQUESTS=10

./online_compiler
```

Or use `.env` file:

```bash
# .env
PORT=3000
DOCKER_TIMEOUT_SECONDS=10
DOCKER_MEMORY_LIMIT=128m
DOCKER_CPU_LIMIT=0.5
DOCKER_PIDS_LIMIT=50
RATE_LIMIT_WINDOW_SECONDS=60
RATE_LIMIT_MAX_REQUESTS=10
```

Then run:
```bash
env $(cat .env) ./online_compiler
```

## Docker Deployment

### Build Image

```bash
cd app
docker build -t online-compiler:latest .
```

**Build stages:**
1. **Builder**: Ubuntu 22.04 + build tools (g++, CMake)
   - Compiles C++ code
   - Fetches dependencies (Crow, SQLite)
2. **Runtime**: Ubuntu 22.04 + Docker CLI
   - Copies compiled binary
   - Minimal size (~500MB)

### Run Container

```bash
docker run -d \
  --name compiler \
  -p 3000:3000 \
  -v /var/run/docker.sock:/var/run/docker.sock \
  -v compiler-data:/app/data \
  online-compiler:latest
```

**Flags:**
- `-d`: Run in background
- `-p 3000:3000`: Map port 3000
- `-v /var/run/docker.sock:/var/run/docker.sock`: Allow container to spawn Docker containers
- `-v compiler-data:/app/data`: Persistent volume for SQLite DB

### Docker Compose

```bash
cd app
docker-compose up -d
```

**docker-compose.yml:**
```yaml
version: '3.8'
services:
  compiler:
    build: .
    ports:
      - "3000:3000"
    volumes:
      - /var/run/docker.sock:/var/run/docker.sock
      - compiler-data:/app/data
    environment:
      PORT: 3000
      DOCKER_TIMEOUT_SECONDS: 10
      DOCKER_MEMORY_LIMIT: 128m
      DOCKER_CPU_LIMIT: 0.5
      DOCKER_PIDS_LIMIT: 50
      RATE_LIMIT_WINDOW_SECONDS: 60
      RATE_LIMIT_MAX_REQUESTS: 10

volumes:
  compiler-data:
```

### View Logs

```bash
docker logs -f compiler
```

### Stop Container

```bash
docker stop compiler
docker rm compiler
```

## Production Deployment

### Recommendations

1. **HTTPS/TLS**
   - Use reverse proxy (nginx, Caddy)
   - Obtain certificate (Let's Encrypt)
   - Redirect HTTP → HTTPS

2. **Authentication**
   - Replace client-side auth with JWT or OAuth
   - Store passwords hashed (bcrypt, Argon2)
   - Add rate limiting on login endpoint

3. **Database**
   - Backup SQLite regularly
   - Consider PostgreSQL for scale
   - Enable encryption at rest

4. **Monitoring**
   - Log all requests (access log)
   - Monitor resource usage (CPU, memory)
   - Alert on errors (500 responses)

5. **Security**
   - Run container as non-root user
   - Use read-only root filesystem
   - Limit Docker daemon access
   - Scan images for vulnerabilities

### Example: nginx Reverse Proxy

```nginx
upstream compiler {
    server localhost:3000;
}

server {
    listen 80;
    server_name compiler.example.com;
    return 301 https://$server_name$request_uri;
}

server {
    listen 443 ssl http2;
    server_name compiler.example.com;

    ssl_certificate /etc/letsencrypt/live/compiler.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/compiler.example.com/privkey.pem;

    location / {
        proxy_pass http://compiler;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_read_timeout 30s;
    }
}
```

### Example: Kubernetes Deployment

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: online-compiler
spec:
  replicas: 2
  selector:
    matchLabels:
      app: online-compiler
  template:
    metadata:
      labels:
        app: online-compiler
    spec:
      containers:
      - name: compiler
        image: online-compiler:latest
        ports:
        - containerPort: 3000
        env:
        - name: PORT
          value: "3000"
        - name: DOCKER_TIMEOUT_SECONDS
          value: "10"
        volumeMounts:
        - name: docker-socket
          mountPath: /var/run/docker.sock
        - name: data
          mountPath: /app/data
        resources:
          requests:
            cpu: 500m
            memory: 512Mi
          limits:
            cpu: 1000m
            memory: 1Gi
      volumes:
      - name: docker-socket
        hostPath:
          path: /var/run/docker.sock
      - name: data
        persistentVolumeClaim:
          claimName: compiler-data

---
apiVersion: v1
kind: Service
metadata:
  name: online-compiler
spec:
  type: LoadBalancer
  ports:
  - port: 80
    targetPort: 3000
  selector:
    app: online-compiler

---
apiVersion: v1
kind: PersistentVolumeClaim
metadata:
  name: compiler-data
spec:
  accessModes:
    - ReadWriteOnce
  resources:
    requests:
      storage: 10Gi
```

## Health Checks

### Docker Health Check

```dockerfile
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
  CMD curl -f http://localhost:3000/public/index.html || exit 1
```

### Manual Health Check

```bash
curl -s http://localhost:3000/public/index.html | head -5
```

Expected: HTML content (200 status)

## Backup & Recovery

### Backup SQLite Database

```bash
# Copy the database file
cp data/submissions.db submissions.db.backup

# Or use SQLite dump
sqlite3 data/submissions.db ".dump" > submissions.sql
```

### Restore from Backup

```bash
# Stop the server
docker stop compiler

# Restore the database
cp submissions.db.backup data/submissions.db

# Start the server
docker start compiler
```

## Monitoring & Logging

### Access Log

Configure nginx to log requests:
```nginx
access_log /var/log/nginx/compiler.access.log;
error_log /var/log/nginx/compiler.error.log;
```

### Application Logs

The server logs to stdout:
```bash
docker logs compiler > compiler.log 2>&1
```

### Metrics to Monitor

- **Request rate**: Requests per second
- **Response time**: P50, P95, P99 latencies
- **Error rate**: 4xx, 5xx responses
- **Docker usage**: Container count, resource usage
- **Database size**: submissions.db file size

## Scaling Considerations

### Horizontal Scaling

1. **Load balancer** (nginx, HAProxy)
2. **Multiple compiler instances** (stateless)
3. **Shared database** (PostgreSQL instead of SQLite)
4. **Shared Docker daemon** or per-instance Docker

### Vertical Scaling

1. Increase container memory/CPU limits
2. Increase Docker resource limits
3. Optimize database queries (add indexes)

### Caching

1. Cache static files (CSS, JS) in CDN
2. Cache `/api/questions` response (rarely changes)
3. Cache language Docker images locally

## Troubleshooting

### Server won't start

```bash
# Check if port 3000 is in use
lsof -i :3000

# Check if Docker is running
docker ps

# Check CMake configuration
cd app/build && cmake ..
```

### Code execution fails

```bash
# Check Docker daemon
docker ps

# Check if images exist
docker images | grep -E "gcc|python|node"

# Check Docker socket permissions
ls -la /var/run/docker.sock
```

### Database errors

```bash
# Check database file
ls -la data/submissions.db

# Check permissions
chmod 666 data/submissions.db

# Verify database integrity
sqlite3 data/submissions.db "PRAGMA integrity_check;"
```

### High memory usage

```bash
# Check Docker container limits
docker stats compiler

# Reduce DOCKER_MEMORY_LIMIT
export DOCKER_MEMORY_LIMIT=64m
```

## Cleanup

### Remove containers

```bash
docker stop compiler
docker rm compiler
```

### Remove images

```bash
docker rmi online-compiler:latest
```

### Remove volumes

```bash
docker volume rm compiler-data
```

### Clean build artifacts

```bash
cd app
rm -rf build
```
