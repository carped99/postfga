#!/bin/bash
# Initialize PostFGA extension in PostgreSQL
# This script runs automatically when the container starts for the first time

set -e

# Wait for PostgreSQL to be ready
until pg_isready -U "${POSTGRES_USER:-postgres}"; do
    echo "Waiting for PostgreSQL to be ready..."
    sleep 1
done

# Create extension if POSTFGA_AUTO_INSTALL is set
if [ "${POSTFGA_AUTO_INSTALL:-false}" = "true" ]; then
    echo "Creating PostFGA extension..."

    # Connect to the default database
    DB="${POSTGRES_DB:-postgres}"

    psql -v ON_ERROR_STOP=1 --username "${POSTGRES_USER:-postgres}" --dbname "$DB" <<-EOSQL
        -- Create extension
        CREATE EXTENSION IF NOT EXISTS postfga;

        -- Verify installation
        SELECT extname, extversion FROM pg_extension WHERE extname = 'postfga';

        \echo 'PostFGA extension installed successfully'
EOSQL

    # Configure GUC parameters if environment variables are set
    if [ -n "$POSTFGA_ENDPOINT" ]; then
        echo "Configuring PostFGA parameters..."

        # Append configuration to postgresql.conf
        cat >> "${PGDATA}/postgresql.conf" <<-EOF

# PostFGA Extension Configuration
# Auto-generated on $(date)
postfga_fdw.endpoint = '${POSTFGA_ENDPOINT}'
postfga_fdw.store_id = '${POSTFGA_STORE_ID}'
postfga_fdw.relations = '${POSTFGA_RELATIONS}'
postfga_fdw.cache_ttl_ms = ${POSTFGA_CACHE_TTL_MS}
postfga_fdw.max_cache_entries = ${POSTFGA_MAX_CACHE_ENTRIES}
postfga_fdw.bgw_workers = ${POSTFGA_BGW_WORKERS}
postfga_fdw.fallback_to_grpc_on_miss = ${POSTFGA_FALLBACK_TO_GRPC}
EOF

        echo "PostFGA configuration added to postgresql.conf"

        # Reload configuration
        psql -v ON_ERROR_STOP=1 --username "${POSTGRES_USER:-postgres}" --dbname "$DB" -c "SELECT pg_reload_conf();"
    fi

    echo "PostFGA initialization complete"
else
    echo "PostFGA auto-install disabled. Set POSTFGA_AUTO_INSTALL=true to enable."
    echo "You can manually create the extension with: CREATE EXTENSION postfga;"
fi
