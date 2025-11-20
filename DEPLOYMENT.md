# PostFGA Deployment Guide

Complete guide for deploying PostgreSQL with the PostFGA extension in production environments.

## 📋 Table of Contents

- [Deployment Options](#deployment-options)
- [Docker Deployment](#docker-deployment)
- [Native Installation](#native-installation)
- [Kubernetes Deployment](#kubernetes-deployment)
- [Configuration](#configuration)
- [Monitoring](#monitoring)
- [Backup & Recovery](#backup--recovery)
- [Security](#security)
- [Troubleshooting](#troubleshooting)

## 🚀 Deployment Options

### 1. Docker (Recommended for Most Use Cases)

**Pros:**
- ✅ Easy to deploy and update
- ✅ Consistent environment
- ✅ Isolated from host system
- ✅ Includes all dependencies

**Cons:**
- ❌ Slight performance overhead
- ❌ Requires Docker knowledge

**Best for:** Development, testing, and production environments with containerized infrastructure.

### 2. Native Installation

**Pros:**
- ✅ Maximum performance
- ✅ Direct system integration
- ✅ Full control over configuration

**Cons:**
- ❌ Manual dependency management
- ❌ Platform-specific builds
- ❌ More complex updates

**Best for:** High-performance production systems, bare-metal servers.

### 3. Kubernetes

**Pros:**
- ✅ Scalable and resilient
- ✅ Automated management
- ✅ Cloud-native integration

**Cons:**
- ❌ Complex setup
- ❌ Requires Kubernetes expertise
- ❌ Higher resource overhead

**Best for:** Large-scale deployments, cloud environments, microservices architectures.

---

## 🐳 Docker Deployment

### Production Setup

```bash
# 1. Clone repository
git clone https://github.com/your-org/postfga.git
cd postfga/docker

# 2. Configure environment
cp .env.example .env
nano .env  # Edit configuration

# 3. Build production image
docker build -f Dockerfile.production -t postfga/postgres:18-1.0.0 ..

# 4. Start services
docker-compose up -d

# 5. Verify installation
docker-compose exec postgres psql -U postgres -c "SELECT * FROM pg_extension WHERE extname='postfga';"
```

### Production Docker Compose

```yaml
version: '3.8'

services:
  postgres:
    image: postfga/postgres:18-1.0.0
    container_name: postfga-postgres-prod
    environment:
      POSTGRES_PASSWORD_FILE: /run/secrets/postgres_password
      POSTFGA_AUTO_INSTALL: "true"
      POSTFGA_ENDPOINT: ${POSTFGA_ENDPOINT}
      POSTFGA_STORE_ID_FILE: /run/secrets/postfga_store_id
    secrets:
      - postgres_password
      - postfga_store_id
    volumes:
      - postgres_data:/var/lib/postgresql/data
      - ./postgresql.conf:/etc/postgresql/postgresql.conf:ro
      - ./backups:/backups
    ports:
      - "5432:5432"
    networks:
      - backend
    deploy:
      resources:
        limits:
          cpus: '4'
          memory: 8G
        reservations:
          cpus: '2'
          memory: 4G
    restart: always
    logging:
      driver: "json-file"
      options:
        max-size: "10m"
        max-file: "3"

secrets:
  postgres_password:
    external: true
  postfga_store_id:
    external: true

volumes:
  postgres_data:
    driver: local

networks:
  backend:
    driver: bridge
```

---

## 💾 Native Installation

### Prerequisites

```bash
# Ubuntu/Debian
apt-get update
apt-get install -y \
    postgresql-18 \
    postgresql-server-dev-18 \
    build-essential \
    cmake \
    ninja-build \
    libgrpc++-dev \
    libprotobuf-dev

# RHEL/CentOS
dnf install -y \
    postgresql18-server \
    postgresql18-devel \
    gcc-c++ \
    cmake \
    ninja-build \
    grpc-devel \
    protobuf-devel
```

### Build and Install

```bash
# 1. Clone and build
git clone https://github.com/your-org/postfga.git
cd postfga
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# 2. Install extension files
sudo cp postfga.so $(pg_config --pkglibdir)/
sudo cp ../sql/postfga.control $(pg_config --sharedir)/extension/
sudo cp ../sql/postfga--1.0.0.sql $(pg_config --sharedir)/extension/

# 3. Set permissions
sudo chmod 755 $(pg_config --pkglibdir)/postfga.so
sudo chmod 644 $(pg_config --sharedir)/extension/postfga*

# 4. Configure PostgreSQL
sudo nano /etc/postgresql/18/main/postgresql.conf
# Add PostFGA configuration (see Configuration section)

# 5. Restart PostgreSQL
sudo systemctl restart postgresql

# 6. Create extension
sudo -u postgres psql -c "CREATE EXTENSION postfga;"
```

---

## ☸️ Kubernetes Deployment

### Helm Chart (Coming Soon)

```bash
# Add Helm repository
helm repo add postfga https://charts.postfga.io
helm repo update

# Install
helm install my-postfga postfga/postfga \
  --set postgresql.password=secret \
  --set postfga.endpoint=openfga-service:8081 \
  --set postfga.storeId=your-store-id
```

### Manual Kubernetes Deployment

```yaml
# postfga-deployment.yaml
apiVersion: apps/v1
kind: StatefulSet
metadata:
  name: postfga-postgres
spec:
  serviceName: postgres
  replicas: 1
  selector:
    matchLabels:
      app: postfga-postgres
  template:
    metadata:
      labels:
        app: postfga-postgres
    spec:
      containers:
      - name: postgres
        image: postfga/postgres:18-1.0.0
        env:
        - name: POSTGRES_PASSWORD
          valueFrom:
            secretKeyRef:
              name: postgres-secret
              key: password
        - name: POSTFGA_AUTO_INSTALL
          value: "true"
        - name: POSTFGA_ENDPOINT
          value: "dns:///openfga-service:8081"
        - name: POSTFGA_STORE_ID
          valueFrom:
            secretKeyRef:
              name: postfga-secret
              key: store-id
        ports:
        - containerPort: 5432
          name: postgres
        volumeMounts:
        - name: postgres-data
          mountPath: /var/lib/postgresql/data
        resources:
          requests:
            memory: "2Gi"
            cpu: "1"
          limits:
            memory: "4Gi"
            cpu: "2"
  volumeClaimTemplates:
  - metadata:
      name: postgres-data
    spec:
      accessModes: [ "ReadWriteOnce" ]
      resources:
        requests:
          storage: 10Gi
---
apiVersion: v1
kind: Service
metadata:
  name: postgres-service
spec:
  selector:
    app: postfga-postgres
  ports:
  - port: 5432
    targetPort: 5432
  type: ClusterIP
```

---

## ⚙️ Configuration

### PostgreSQL Configuration (postgresql.conf)

```ini
# Connection Settings
listen_addresses = '*'
max_connections = 200

# Memory Settings
shared_buffers = 2GB
effective_cache_size = 6GB
maintenance_work_mem = 512MB
work_mem = 16MB

# Write-Ahead Log
wal_level = replica
max_wal_size = 2GB
min_wal_size = 1GB

# PostFGA Extension
postfga_fdw.endpoint = 'dns:///openfga:8081'
postfga_fdw.store_id = 'your-store-id'
postfga_fdw.relations = 'read,write,edit,delete,download,owner'
postfga_fdw.cache_ttl_ms = 60000
postfga_fdw.max_cache_entries = 10000
postfga_fdw.bgw_workers = 1
postfga_fdw.fallback_to_grpc_on_miss = true

# Logging
log_destination = 'stderr'
logging_collector = on
log_directory = 'log'
log_filename = 'postgresql-%Y-%m-%d_%H%M%S.log'
log_min_duration_statement = 1000  # Log slow queries (>1s)
```

### pg_hba.conf (Authentication)

```conf
# TYPE  DATABASE        USER            ADDRESS                 METHOD

# Local connections
local   all             postgres                                peer
local   all             all                                     md5

# IPv4 connections
host    all             all             0.0.0.0/0               scram-sha-256

# IPv6 connections
host    all             all             ::/0                    scram-sha-256

# Replication
host    replication     all             0.0.0.0/0               scram-sha-256
```

---

## 📊 Monitoring

### Prometheus Metrics (Future)

```yaml
# prometheus.yml
scrape_configs:
  - job_name: 'postfga'
    static_configs:
      - targets: ['postgres:9187']
```

### Health Checks

```bash
# PostgreSQL health
pg_isready -h localhost -U postgres

# Extension status
psql -U postgres -c "SELECT * FROM pg_extension WHERE extname='postfga';"

# Cache statistics
psql -U postgres -c "SELECT * FROM postfga_fdw.cache_stats();"
```

### Logging

Monitor these log patterns:

```bash
# Error patterns
grep -i "error\|fatal\|panic" /var/log/postgresql/postgresql-*.log

# PostFGA-specific logs
grep -i "postfga" /var/log/postgresql/postgresql-*.log

# Slow queries
grep "duration:" /var/log/postgresql/postgresql-*.log | awk '$10 > 1000'
```

---

## 💾 Backup & Recovery

### Automated Backups

```bash
#!/bin/bash
# backup-postfga.sh

BACKUP_DIR="/backups"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
BACKUP_FILE="${BACKUP_DIR}/postfga_${TIMESTAMP}.sql.gz"

# Backup
pg_dump -U postgres your_database | gzip > "${BACKUP_FILE}"

# Keep only last 7 days
find ${BACKUP_DIR} -name "postfga_*.sql.gz" -mtime +7 -delete

echo "Backup completed: ${BACKUP_FILE}"
```

### Restore

```bash
# Decompress and restore
gunzip -c backup.sql.gz | psql -U postgres your_database
```

### Point-in-Time Recovery (PITR)

Configure in `postgresql.conf`:

```ini
wal_level = replica
archive_mode = on
archive_command = 'cp %p /archive/%f'
```

---

## 🔐 Security

### SSL/TLS Configuration

```ini
# postgresql.conf
ssl = on
ssl_cert_file = '/var/lib/postgresql/server.crt'
ssl_key_file = '/var/lib/postgresql/server.key'
ssl_ca_file = '/var/lib/postgresql/root.crt'
ssl_ciphers = 'HIGH:MEDIUM:+3DES:!aNULL'
ssl_prefer_server_ciphers = on
```

### User Management

```sql
-- Create read-only user
CREATE USER readonly_user WITH PASSWORD 'secure_password';
GRANT CONNECT ON DATABASE your_database TO readonly_user;
GRANT USAGE ON SCHEMA public TO readonly_user;
GRANT SELECT ON ALL TABLES IN SCHEMA public TO readonly_user;

-- Create application user
CREATE USER app_user WITH PASSWORD 'secure_password';
GRANT CONNECT ON DATABASE your_database TO app_user;
GRANT USAGE ON FOREIGN DATA WRAPPER postfga_fdw TO app_user;
```

---

## 🐛 Troubleshooting

See [INSTALLATION_AND_TESTING.md](INSTALLATION_AND_TESTING.md#troubleshooting) for detailed troubleshooting steps.

---

## 📚 Additional Resources

- [Installation Guide](INSTALLATION_AND_TESTING.md)
- [Docker Documentation](docker/README.md)
- [Architecture Overview](ARCHITECTURE.md)
- [Development Guide](DEVELOPMENT.md)
