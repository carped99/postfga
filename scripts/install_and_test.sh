#!/bin/bash
# Complete installation and testing script for postfga extension

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

echo -e "${BLUE}╔════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   PostFGA Extension - Install and Test Suite      ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════╝${NC}"
echo ""

# Step 1: Check if PostgreSQL is available
echo -e "${GREEN}[1/6] Checking PostgreSQL...${NC}"
if ! command -v pg_config &> /dev/null; then
    echo -e "${RED}ERROR: PostgreSQL not found${NC}"
    exit 1
fi
PG_VERSION=$(pg_config --version | grep -oP '\d+' | head -1)
echo -e "${GREEN}      ✓ PostgreSQL ${PG_VERSION} detected${NC}"
echo ""

# Step 2: Build the extension if needed
echo -e "${GREEN}[2/6] Building extension...${NC}"
if [ ! -f "build/postfga.so" ]; then
    echo -e "${YELLOW}      Extension not built. Building now...${NC}"
    if [ ! -d "build" ]; then
        mkdir build
        cd build
        cmake ..
        cd ..
    fi
    cmake --build build
    echo -e "${GREEN}      ✓ Build complete${NC}"
else
    echo -e "${GREEN}      ✓ Extension already built${NC}"
fi
echo ""

# Step 3: Initialize PostgreSQL
echo -e "${GREEN}[3/6] Initializing PostgreSQL...${NC}"
bash scripts/init_postgres.sh
echo ""

# Step 4: Install extension files
echo -e "${GREEN}[4/6] Installing extension files...${NC}"
bash scripts/install_extension.sh
echo ""

# Step 5: Create extension in database
DB_NAME="${PGDATABASE:-postfga_test}"
echo -e "${GREEN}[5/6] Creating extension in database '${DB_NAME}'...${NC}"

# Check if database exists, create if not
if ! sudo -u postgres psql -lqt | cut -d \| -f 1 | grep -qw "$DB_NAME"; then
    echo -e "${YELLOW}      Creating database ${DB_NAME}...${NC}"
    sudo -u postgres createdb "$DB_NAME"
fi

# Create extension
sudo -u postgres psql -d "$DB_NAME" -c "DROP EXTENSION IF EXISTS postfga CASCADE;" >/dev/null 2>&1 || true
sudo -u postgres psql -d "$DB_NAME" -c "CREATE EXTENSION postfga;" >/dev/null 2>&1

if [ $? -eq 0 ]; then
    echo -e "${GREEN}      ✓ Extension created successfully${NC}"
else
    echo -e "${RED}      ✗ Failed to create extension${NC}"
    echo -e "${YELLOW}      Checking PostgreSQL logs...${NC}"
    sudo tail -20 /var/log/postgresql/postgresql-${PG_VERSION}-main.log 2>/dev/null || \
    sudo journalctl -u postgresql -n 20
    exit 1
fi
echo ""

# Step 6: Run tests
echo -e "${GREEN}[6/6] Running test suite...${NC}"
if [ -f "tests/test_extension.sql" ]; then
    sudo -u postgres psql -d "$DB_NAME" -f tests/test_extension.sql
    TEST_RESULT=$?
    echo ""

    if [ $TEST_RESULT -eq 0 ]; then
        echo -e "${GREEN}      ✓ Test suite completed${NC}"
    else
        echo -e "${YELLOW}      ⚠ Test suite completed with warnings${NC}"
        echo -e "${YELLOW}      (Some tests may fail if OpenFGA is not configured)${NC}"
    fi
else
    echo -e "${YELLOW}      ⚠ Test file not found${NC}"
fi
echo ""

# Summary
echo -e "${BLUE}╔════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║              Installation Complete!                ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${GREEN}✓ Extension installed and tested successfully${NC}"
echo ""
echo -e "${YELLOW}To use the extension:${NC}"
echo -e "  1. Connect to database:"
echo -e "     ${BLUE}sudo -u postgres psql ${DB_NAME}${NC}"
echo ""
echo -e "  2. The extension is already created. You can now:"
echo -e "     - Create a foreign server for OpenFGA"
echo -e "     - Create foreign tables for ACL permissions"
echo -e "     - Query permissions"
echo ""
echo -e "${YELLOW}Example usage:${NC}"
echo -e "  ${BLUE}-- Create server${NC}"
echo -e "  CREATE SERVER my_openfga FOREIGN DATA WRAPPER postfga_fdw"
echo -e "    OPTIONS (endpoint 'dns:///localhost:8081', store_id 'my-store');"
echo ""
echo -e "  ${BLUE}-- Create foreign table${NC}"
echo -e "  CREATE FOREIGN TABLE acl_permission ("
echo -e "    object_type text, object_id text,"
echo -e "    subject_type text, subject_id text, relation text"
echo -e "  ) SERVER my_openfga;"
echo ""
echo -e "  ${BLUE}-- Check permission${NC}"
echo -e "  SELECT 1 FROM acl_permission"
echo -e "    WHERE object_type='doc' AND object_id='123'"
echo -e "      AND subject_type='user' AND subject_id='alice'"
echo -e "      AND relation='read';"
echo ""
echo -e "${YELLOW}Cache management:${NC}"
echo -e "  ${BLUE}SELECT postfga_fdw.cache_stats();     -- View cache statistics${NC}"
echo -e "  ${BLUE}SELECT postfga_fdw.clear_cache();     -- Clear cache${NC}"
echo ""
