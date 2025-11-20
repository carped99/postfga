#!/bin/bash
# Test PostFGA Docker deployment

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

IMAGE_NAME="${1:-postfga/postgres:18-1.0.0}"
CONTAINER_NAME="postfga-test-$$"
TEST_DB="test_db"
TEST_PASSWORD="test_password_$(date +%s)"

echo -e "${BLUE}╔════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║        PostFGA Docker Image Test Suite            ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${GREEN}Testing image: ${YELLOW}${IMAGE_NAME}${NC}"
echo ""

# Cleanup function
cleanup() {
    echo ""
    echo -e "${YELLOW}Cleaning up...${NC}"
    docker stop "$CONTAINER_NAME" >/dev/null 2>&1 || true
    docker rm "$CONTAINER_NAME" >/dev/null 2>&1 || true
    echo -e "${GREEN}✓ Cleanup complete${NC}"
}

trap cleanup EXIT

# Test 1: Image exists
echo -e "${GREEN}[1/8] Checking if image exists...${NC}"
if docker image inspect "$IMAGE_NAME" >/dev/null 2>&1; then
    echo -e "${GREEN}      ✓ Image found${NC}"
else
    echo -e "${RED}      ✗ Image not found${NC}"
    echo -e "${YELLOW}      Build the image first: ./scripts/build_docker.sh --prod${NC}"
    exit 1
fi

# Test 2: Start container
echo -e "${GREEN}[2/8] Starting container...${NC}"
docker run -d \
    --name "$CONTAINER_NAME" \
    -e POSTGRES_PASSWORD="$TEST_PASSWORD" \
    -e POSTGRES_DB="$TEST_DB" \
    -e POSTFGA_AUTO_INSTALL=true \
    -e POSTFGA_ENDPOINT=dns:///localhost:8081 \
    -e POSTFGA_STORE_ID=test-store \
    "$IMAGE_NAME" >/dev/null

echo -e "${GREEN}      ✓ Container started${NC}"

# Test 3: Wait for PostgreSQL to be ready
echo -e "${GREEN}[3/8] Waiting for PostgreSQL to be ready...${NC}"
for i in {1..30}; do
    if docker exec "$CONTAINER_NAME" pg_isready -U postgres >/dev/null 2>&1; then
        echo -e "${GREEN}      ✓ PostgreSQL is ready (${i}s)${NC}"
        break
    fi
    if [ $i -eq 30 ]; then
        echo -e "${RED}      ✗ Timeout waiting for PostgreSQL${NC}"
        docker logs "$CONTAINER_NAME"
        exit 1
    fi
    sleep 1
done

# Test 4: Check PostgreSQL version
echo -e "${GREEN}[4/8] Checking PostgreSQL version...${NC}"
PG_VERSION=$(docker exec "$CONTAINER_NAME" psql -U postgres -t -c "SELECT version();" | head -1)
echo -e "${GREEN}      ✓ ${PG_VERSION}${NC}"

# Test 5: Check if extension files exist
echo -e "${GREEN}[5/8] Checking extension files...${NC}"
docker exec "$CONTAINER_NAME" ls -lh "$(docker exec "$CONTAINER_NAME" pg_config --pkglibdir)/postfga.so" >/dev/null
echo -e "${GREEN}      ✓ postfga.so found${NC}"

docker exec "$CONTAINER_NAME" ls -lh "$(docker exec "$CONTAINER_NAME" pg_config --sharedir)/extension/postfga.control" >/dev/null
echo -e "${GREEN}      ✓ postfga.control found${NC}"

docker exec "$CONTAINER_NAME" ls -lh "$(docker exec "$CONTAINER_NAME" pg_config --sharedir)/extension/postfga--1.0.0.sql" >/dev/null
echo -e "${GREEN}      ✓ postfga--1.0.0.sql found${NC}"

# Test 6: Check if extension is installed (if auto-install enabled)
echo -e "${GREEN}[6/8] Checking extension installation...${NC}"
EXTENSION_COUNT=$(docker exec "$CONTAINER_NAME" psql -U postgres -d "$TEST_DB" -t -c "SELECT COUNT(*) FROM pg_extension WHERE extname='postfga';" | tr -d ' ')

if [ "$EXTENSION_COUNT" = "1" ]; then
    echo -e "${GREEN}      ✓ Extension auto-installed${NC}"
    EXTENSION_VERSION=$(docker exec "$CONTAINER_NAME" psql -U postgres -d "$TEST_DB" -t -c "SELECT extversion FROM pg_extension WHERE extname='postfga';" | tr -d ' ')
    echo -e "${GREEN}        Version: ${EXTENSION_VERSION}${NC}"
else
    echo -e "${YELLOW}      ! Extension not auto-installed (trying manual install)${NC}"
    docker exec "$CONTAINER_NAME" psql -U postgres -d "$TEST_DB" -c "CREATE EXTENSION postfga;" >/dev/null
    echo -e "${GREEN}      ✓ Extension manually installed${NC}"
fi

# Test 7: Check FDW
echo -e "${GREEN}[7/8] Checking Foreign Data Wrapper...${NC}"
FDW_COUNT=$(docker exec "$CONTAINER_NAME" psql -U postgres -d "$TEST_DB" -t -c "SELECT COUNT(*) FROM pg_foreign_data_wrapper WHERE fdwname='postfga_fdw';" | tr -d ' ')

if [ "$FDW_COUNT" = "1" ]; then
    echo -e "${GREEN}      ✓ postfga_fdw found${NC}"
else
    echo -e "${RED}      ✗ postfga_fdw not found${NC}"
    exit 1
fi

# Test 8: Test basic functionality
echo -e "${GREEN}[8/8] Testing basic functionality...${NC}"

# Create test server
docker exec "$CONTAINER_NAME" psql -U postgres -d "$TEST_DB" -c "
    DROP SERVER IF EXISTS test_server CASCADE;
    CREATE SERVER test_server FOREIGN DATA WRAPPER postfga_fdw
    OPTIONS (endpoint 'dns:///localhost:8081', store_id 'test-store');
" >/dev/null
echo -e "${GREEN}      ✓ Test server created${NC}"

# Create foreign table
docker exec "$CONTAINER_NAME" psql -U postgres -d "$TEST_DB" -c "
    CREATE FOREIGN TABLE test_acl_permission (
        object_type text,
        object_id text,
        subject_type text,
        subject_id text,
        relation text
    ) SERVER test_server;
" >/dev/null
echo -e "${GREEN}      ✓ Foreign table created${NC}"

# Test cache functions
CACHE_STATS=$(docker exec "$CONTAINER_NAME" psql -U postgres -d "$TEST_DB" -t -c "SELECT COUNT(*) FROM postfga_fdw.cache_stats();" | tr -d ' ')
if [ "$CACHE_STATS" = "1" ]; then
    echo -e "${GREEN}      ✓ Cache stats function works${NC}"
else
    echo -e "${RED}      ✗ Cache stats function failed${NC}"
    exit 1
fi

docker exec "$CONTAINER_NAME" psql -U postgres -d "$TEST_DB" -c "SELECT postfga_fdw.clear_cache();" >/dev/null
echo -e "${GREEN}      ✓ Clear cache function works${NC}"

# Success summary
echo ""
echo -e "${BLUE}╔════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║           All Tests Passed! ✓                      ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${GREEN}Summary:${NC}"
echo -e "  Image:      ${YELLOW}${IMAGE_NAME}${NC}"
echo -e "  Container:  ${YELLOW}${CONTAINER_NAME}${NC}"
echo -e "  PostgreSQL: ${YELLOW}Ready${NC}"
echo -e "  Extension:  ${YELLOW}Installed and working${NC}"
echo ""
echo -e "${YELLOW}The test container is still running.${NC}"
echo -e "${YELLOW}To connect:${NC}"
echo -e "  docker exec -it ${CONTAINER_NAME} psql -U postgres -d ${TEST_DB}"
echo ""
echo -e "${YELLOW}To stop and remove:${NC}"
echo -e "  docker stop ${CONTAINER_NAME} && docker rm ${CONTAINER_NAME}"
echo ""
echo -e "${GREEN}Or just exit this script and it will auto-cleanup.${NC}"
echo ""

# Keep container running for manual inspection
read -p "Press Enter to cleanup and exit..."
