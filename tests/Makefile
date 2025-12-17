# Makefile for PostFGA tests

MODULE_big = test_guc
OBJS = test_guc.o

# PostgreSQL extension settings
EXTENSION = fga_fdw
DATA = sql/guc_test.sql sql/guc_pgtap_test.sql

# Regression test settings
REGRESS = guc_test
REGRESS_OPTS = --inputdir=./ --load-extension=$(EXTENSION)

# Include path for src directory
PG_CPPFLAGS = -I../src

# PostgreSQL build system
PG_CONFIG ?= pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

# Additional targets
.PHONY: test test-sql test-unit test-pgtap clean-test

# Run all tests
test: test-sql test-unit

# Run SQL-based regression tests
test-sql:
	@echo "=== Running SQL Regression Tests ==="
	$(MAKE) installcheck

# Run unit tests (requires building the test module)
test-unit: test_guc.so
	@echo "=== Running Unit Tests ==="
	@echo "Note: Unit tests require PostgreSQL server to be running"
	psql -d postgres -c "CREATE EXTENSION IF NOT EXISTS fga_fdw;"
	psql -d postgres -c "DROP FUNCTION IF EXISTS test_guc_run();"
	psql -d postgres -c "CREATE FUNCTION test_guc_run() RETURNS BOOLEAN AS '$(shell pwd)/test_guc', 'test_guc_run' LANGUAGE C STRICT;"
	psql -d postgres -c "SELECT test_guc_run();"

# Run pgTAP tests (requires pgTAP extension)
test-pgtap:
	@echo "=== Running pgTAP Tests ==="
	@which pg_prove > /dev/null || (echo "pg_prove not found. Install pgTAP." && exit 1)
	pg_prove -d postgres sql/guc_pgtap_test.sql

# Manual SQL test
test-manual:
	@echo "=== Running Manual SQL Test ==="
	psql -d postgres -f sql/guc_test.sql

# Clean test artifacts
clean-test:
	rm -rf results/ regression.diffs regression.out tmp_check/
	rm -f test_guc.o test_guc.so

# Help
help:
	@echo "PostFGA Test Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  test         - Run all tests"
	@echo "  test-sql     - Run SQL regression tests"
	@echo "  test-unit    - Run C unit tests"
	@echo "  test-pgtap   - Run pgTAP tests"
	@echo "  test-manual  - Run manual SQL test"
	@echo "  clean-test   - Clean test artifacts"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make test              # Run all tests"
	@echo "  make test-sql          # Run SQL tests only"
	@echo "  make PG_CONFIG=/usr/local/pgsql/bin/pg_config test"
