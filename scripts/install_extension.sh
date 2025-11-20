#!/bin/bash
# Install postfga extension to PostgreSQL

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== PostFGA Extension Installation ===${NC}"

# Detect PostgreSQL version
if command -v pg_config &> /dev/null; then
    PG_VERSION=$(pg_config --version | grep -oP '\d+' | head -1)
    PG_LIBDIR=$(pg_config --pkglibdir)
    PG_SHAREDIR=$(pg_config --sharedir)
    echo -e "${GREEN}Detected PostgreSQL version: ${PG_VERSION}${NC}"
else
    echo -e "${RED}ERROR: pg_config not found. Is PostgreSQL installed?${NC}"
    exit 1
fi

# Project directories
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
SQL_DIR="${PROJECT_ROOT}/sql"

echo -e "${YELLOW}Project root: ${PROJECT_ROOT}${NC}"
echo -e "${YELLOW}Build directory: ${BUILD_DIR}${NC}"

# Check if build files exist
if [ ! -f "${BUILD_DIR}/postfga.so" ]; then
    echo -e "${RED}ERROR: postfga.so not found in ${BUILD_DIR}${NC}"
    echo -e "${YELLOW}Please run 'cmake --build build' first${NC}"
    exit 1
fi

if [ ! -f "${SQL_DIR}/postfga.control" ]; then
    echo -e "${RED}ERROR: postfga.control not found in ${SQL_DIR}${NC}"
    exit 1
fi

if [ ! -f "${SQL_DIR}/postfga--1.0.0.sql" ]; then
    echo -e "${RED}ERROR: postfga--1.0.0.sql not found in ${SQL_DIR}${NC}"
    exit 1
fi

# Install files
echo -e "${GREEN}Installing extension files...${NC}"

# Copy library
echo -e "${YELLOW}Installing library: ${PG_LIBDIR}/postfga.so${NC}"
sudo cp "${BUILD_DIR}/postfga.so" "${PG_LIBDIR}/postfga.so"
sudo chmod 755 "${PG_LIBDIR}/postfga.so"

# Copy control file
EXTENSION_DIR="${PG_SHAREDIR}/extension"
echo -e "${YELLOW}Installing control file: ${EXTENSION_DIR}/postfga.control${NC}"
sudo mkdir -p "${EXTENSION_DIR}"
sudo cp "${SQL_DIR}/postfga.control" "${EXTENSION_DIR}/postfga.control"
sudo chmod 644 "${EXTENSION_DIR}/postfga.control"

# Copy SQL file
echo -e "${YELLOW}Installing SQL file: ${EXTENSION_DIR}/postfga--1.0.0.sql${NC}"
sudo cp "${SQL_DIR}/postfga--1.0.0.sql" "${EXTENSION_DIR}/postfga--1.0.0.sql"
sudo chmod 644 "${EXTENSION_DIR}/postfga--1.0.0.sql"

echo -e "${GREEN}=== Installation Complete ===${NC}"
echo -e "${YELLOW}Files installed:${NC}"
echo -e "  Library:      ${PG_LIBDIR}/postfga.so"
echo -e "  Control file: ${EXTENSION_DIR}/postfga.control"
echo -e "  SQL file:     ${EXTENSION_DIR}/postfga--1.0.0.sql"
echo ""
echo -e "${GREEN}You can now create the extension in your database:${NC}"
echo -e "${YELLOW}  CREATE EXTENSION postfga;${NC}"
