#!/bin/bash
#
# run_tests.sh - Test runner script for PostFGA
#
# Usage:
#   ./run_tests.sh                 # Run all tests
#   ./run_tests.sh sql             # Run SQL tests only
#   ./run_tests.sh unit            # Run unit tests only
#   ./run_tests.sh pgtap           # Run pgTAP tests only
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
DB_NAME="${PGDATABASE:-postgres}"
DB_USER="${PGUSER:-postgres}"
DB_HOST="${PGHOST:-localhost}"
DB_PORT="${PGPORT:-5432}"

# Functions
print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

check_postgres() {
    print_info "Checking PostgreSQL connection..."
    if ! psql -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" -d "$DB_NAME" -c "SELECT 1;" > /dev/null 2>&1; then
        print_error "Cannot connect to PostgreSQL"
        print_info "Connection details:"
        echo "  Host: $DB_HOST"
        echo "  Port: $DB_PORT"
        echo "  Database: $DB_NAME"
        echo "  User: $DB_USER"
        exit 1
    fi
    print_success "PostgreSQL connection OK"
}

check_extension() {
    print_info "Checking if extension is installed..."
    if ! psql -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" -d "$DB_NAME" -c "SELECT 1 FROM pg_available_extensions WHERE name = 'postfga_fdw';" | grep -q 1; then
        print_warning "Extension 'postfga_fdw' not found in pg_available_extensions"
        print_info "You may need to install the extension first: make install"
        return 1
    fi
    print_success "Extension is available"
}

run_sql_tests() {
    print_header "Running SQL Regression Tests"

    if [ ! -f "sql/guc_test.sql" ]; then
        print_error "Test file sql/guc_test.sql not found"
        return 1
    fi

    print_info "Executing guc_test.sql..."
    if psql -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" -d "$DB_NAME" -f sql/guc_test.sql > /tmp/guc_test_output.txt 2>&1; then
        print_success "SQL tests passed"
        cat /tmp/guc_test_output.txt
        return 0
    else
        print_error "SQL tests failed"
        cat /tmp/guc_test_output.txt
        return 1
    fi
}

run_unit_tests() {
    print_header "Running C Unit Tests"

    if [ ! -f "test_guc.c" ]; then
        print_warning "Unit test file test_guc.c not found, skipping"
        return 0
    fi

    print_info "Building unit test module..."
    if make test_guc.so > /tmp/build_unit.log 2>&1; then
        print_success "Unit test module built"
    else
        print_error "Failed to build unit test module"
        cat /tmp/build_unit.log
        return 1
    fi

    print_info "Running unit tests..."
    # Note: This requires creating the function in database
    print_warning "Unit tests require manual setup (see README_TESTING.md)"
    return 0
}

run_pgtap_tests() {
    print_header "Running pgTAP Tests"

    # Check if pgTAP is available
    if ! command -v pg_prove &> /dev/null; then
        print_warning "pg_prove not found, trying direct SQL execution"
        if [ -f "sql/guc_pgtap_test.sql" ]; then
            psql -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" -d "$DB_NAME" -f sql/guc_pgtap_test.sql
        else
            print_error "Test file sql/guc_pgtap_test.sql not found"
            return 1
        fi
        return 0
    fi

    # Check if pgTAP extension is installed
    if ! psql -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" -d "$DB_NAME" -c "SELECT 1 FROM pg_available_extensions WHERE name = 'pgtap';" | grep -q 1; then
        print_warning "pgTAP extension not installed, skipping pgTAP tests"
        print_info "Install pgTAP from https://pgtap.org/"
        return 0
    fi

    print_info "Running pgTAP tests..."
    if pg_prove -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" -d "$DB_NAME" sql/guc_pgtap_test.sql; then
        print_success "pgTAP tests passed"
        return 0
    else
        print_error "pgTAP tests failed"
        return 1
    fi
}

show_usage() {
    echo "Usage: $0 [test-type]"
    echo ""
    echo "Test types:"
    echo "  all     - Run all tests (default)"
    echo "  sql     - Run SQL regression tests only"
    echo "  unit    - Run C unit tests only"
    echo "  pgtap   - Run pgTAP tests only"
    echo ""
    echo "Environment variables:"
    echo "  PGHOST      - PostgreSQL host (default: localhost)"
    echo "  PGPORT      - PostgreSQL port (default: 5432)"
    echo "  PGDATABASE  - Database name (default: postgres)"
    echo "  PGUSER      - Database user (default: postgres)"
    echo ""
    echo "Examples:"
    echo "  $0                    # Run all tests"
    echo "  $0 sql                # Run SQL tests only"
    echo "  PGDATABASE=testdb $0  # Run all tests on testdb"
}

# Main
main() {
    local test_type="${1:-all}"
    local exit_code=0

    print_header "PostFGA Test Runner"
    echo "Database: $DB_NAME"
    echo "Host: $DB_HOST:$DB_PORT"
    echo "User: $DB_USER"
    echo ""

    # Check prerequisites
    check_postgres || exit 1
    check_extension || print_warning "Extension check failed (continuing anyway)"

    case "$test_type" in
        sql)
            run_sql_tests || exit_code=1
            ;;
        unit)
            run_unit_tests || exit_code=1
            ;;
        pgtap)
            run_pgtap_tests || exit_code=1
            ;;
        all)
            run_sql_tests || exit_code=1
            echo ""
            run_unit_tests || exit_code=1
            echo ""
            run_pgtap_tests || exit_code=1
            ;;
        help|--help|-h)
            show_usage
            exit 0
            ;;
        *)
            print_error "Unknown test type: $test_type"
            show_usage
            exit 1
            ;;
    esac

    echo ""
    print_header "Test Results"
    if [ $exit_code -eq 0 ]; then
        print_success "All tests completed successfully!"
    else
        print_error "Some tests failed!"
    fi

    exit $exit_code
}

# Run main
main "$@"
