#!/bin/bash
# Initialize PostgreSQL for postfga extension development/testing

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== PostgreSQL Initialization ===${NC}"

# Check if PostgreSQL is installed
if ! command -v pg_config &> /dev/null; then
    echo -e "${RED}ERROR: PostgreSQL is not installed or pg_config not in PATH${NC}"
    exit 1
fi

PG_VERSION=$(pg_config --version | grep -oP '\d+' | head -1)
echo -e "${GREEN}PostgreSQL version: ${PG_VERSION}${NC}"

# Check if PostgreSQL service is running
if systemctl is-active --quiet postgresql 2>/dev/null; then
    echo -e "${GREEN}PostgreSQL service is running${NC}"
elif service postgresql status >/dev/null 2>&1; then
    echo -e "${GREEN}PostgreSQL service is running${NC}"
else
    echo -e "${YELLOW}PostgreSQL service is not running${NC}"
    echo -e "${YELLOW}Attempting to start PostgreSQL...${NC}"

    # Try different methods to start PostgreSQL
    if command -v systemctl &> /dev/null; then
        sudo systemctl start postgresql || true
    elif command -v service &> /dev/null; then
        sudo service postgresql start || true
    elif command -v pg_ctl &> /dev/null; then
        # For development environments (like devcontainer)
        PG_DATA="${PGDATA:-/var/lib/postgresql/${PG_VERSION}/main}"
        if [ ! -d "$PG_DATA" ]; then
            echo -e "${YELLOW}Initializing database cluster in ${PG_DATA}...${NC}"
            sudo mkdir -p "$PG_DATA"
            sudo chown -R postgres:postgres "$PG_DATA"
            sudo -u postgres pg_ctl init -D "$PG_DATA"
        fi
        sudo -u postgres pg_ctl start -D "$PG_DATA" -l /var/log/postgresql/postgresql.log || true
    fi

    # Wait a moment for PostgreSQL to start
    sleep 2
fi

# Verify PostgreSQL is responding
if sudo -u postgres psql -c "SELECT version();" >/dev/null 2>&1; then
    echo -e "${GREEN}PostgreSQL is responding to queries${NC}"
else
    echo -e "${RED}ERROR: PostgreSQL is not responding${NC}"
    echo -e "${YELLOW}Please check PostgreSQL logs and ensure the service is running${NC}"
    exit 1
fi

# Create test database if it doesn't exist
DB_NAME="${PGDATABASE:-fga_test}"
echo -e "${YELLOW}Checking for test database: ${DB_NAME}${NC}"

if sudo -u postgres psql -lqt | cut -d \| -f 1 | grep -qw "$DB_NAME"; then
    echo -e "${GREEN}Database '${DB_NAME}' already exists${NC}"
else
    echo -e "${YELLOW}Creating database '${DB_NAME}'...${NC}"
    sudo -u postgres createdb "$DB_NAME"
    echo -e "${GREEN}Database '${DB_NAME}' created${NC}"
fi

# Set up GUC parameters for postfga (optional)
echo -e "${YELLOW}Setting up PostgreSQL configuration for postfga...${NC}"
PG_CONF="/etc/postgresql/${PG_VERSION}/main/postgresql.conf"

if [ -f "$PG_CONF" ]; then
    # Backup original config
    if [ ! -f "${PG_CONF}.backup" ]; then
        sudo cp "$PG_CONF" "${PG_CONF}.backup"
        echo -e "${GREEN}Backed up original config to ${PG_CONF}.backup${NC}"
    fi

    # Add postfga configuration (if not already present)
    if ! grep -q "fga_fdw" "$PG_CONF"; then
        echo -e "${YELLOW}Adding postfga configuration to postgresql.conf...${NC}"
        sudo tee -a "$PG_CONF" > /dev/null <<EOF

# PostFGA Extension Configuration
# Added by init_postgres.sh on $(date)
#fga_fdw.endpoint = 'dns:///localhost:8081'
#fga_fdw.store_id = 'your-store-id'
#fga_fdw.relations = 'read,write,edit,delete,download,owner'
#fga_fdw.cache_ttl_ms = 60000
#fga_fdw.max_cache_entries = 10000
#fga_fdw.bgw_workers = 1
#fga_fdw.fallback_to_grpc_on_miss = true
EOF
        echo -e "${GREEN}Configuration added (commented out - edit as needed)${NC}"
        echo -e "${YELLOW}Note: You'll need to restart PostgreSQL for changes to take effect${NC}"
    else
        echo -e "${GREEN}PostFGA configuration already present in postgresql.conf${NC}"
    fi
else
    echo -e "${YELLOW}Warning: Could not find postgresql.conf at ${PG_CONF}${NC}"
    echo -e "${YELLOW}You may need to manually configure GUC parameters${NC}"
fi

echo -e "${GREEN}=== PostgreSQL Initialization Complete ===${NC}"
echo -e "${YELLOW}Database: ${DB_NAME}${NC}"
echo -e "${YELLOW}Next steps:${NC}"
echo -e "  1. Run './scripts/install_extension.sh' to install the extension"
echo -e "  2. Connect to database: sudo -u postgres psql ${DB_NAME}"
echo -e "  3. Create extension: CREATE EXTENSION postfga;"
echo -e "  4. Run tests: \\i tests/test_extension.sql"
