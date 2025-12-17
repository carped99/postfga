#!/bin/bash
set -euo pipefail

EXT_NAME="postfga"

# Get current shared_preload_libraries
CUR_LIBS=$("${psql[@]}" -tAc "SHOW shared_preload_libraries" 2>/dev/null | tr -d ' ')
if [[ ",$CUR_LIBS," != *",$EXT_NAME,"* ]]; then
    echo "Configuring shared_preload_libraries to include ${EXT_NAME}"

    NEW_LIBS="${CUR_LIBS:+$CUR_LIBS,}${EXT_NAME}"
    "${psql[@]}" -q -c "ALTER SYSTEM SET shared_preload_libraries='${NEW_LIBS}';"

    pg_ctl -D "$PGDATA" -m fast -w restart
fi

for DB in "$POSTGRES_DB"; do
	echo "Loading PostFGA extensions into $DB"
	"${psql[@]}" --dbname="$DB" <<-'EOSQL'
		CREATE EXTENSION IF NOT EXISTS postfga;
EOSQL
done

if [[ -n "${FGA_ENDPOINT:-}" ]]; then
    echo "Configuring postfga.endpoint = ${FGA_ENDPOINT}"
    "${psql[@]}" -q -c "ALTER SYSTEM SET postfga.endpoint = \$\$${FGA_ENDPOINT}\$\$;"
fi

if [[ -n "${FGA_STORE_ID:-}" ]]; then
    echo "Configuring postfga.store_id = ${FGA_STORE_ID}"
    "${psql[@]}" -q -c "ALTER SYSTEM SET postfga.store_id = \$\$${FGA_STORE_ID}\$\$;"
fi

if [[ -n "${FGA_MODEL_ID:-}" ]]; then
    echo "Configuring postfga.model_id = ${FGA_MODEL_ID}"
    "${psql[@]}" -q -c "ALTER SYSTEM SET postfga.model_id = \$\$${FGA_MODEL_ID}\$\$;"
fi

# -- Initialize postfga extension on startup

# -- Create extension
# CREATE EXTENSION IF NOT EXISTS postfga;

# -- Create test server
# CREATE SERVER IF NOT EXISTS postfga_server
#   FOREIGN DATA WRAPPER postfga_fdw
#   OPTIONS (
#     endpoint 'dns:///openfga:8081',
#     store_id 'test-store'
#   );

# -- Create foreign table for testing
# CREATE FOREIGN TABLE IF NOT EXISTS acl_permission (
#     object_type text,
#     object_id text,
#     subject_type text,
#     subject_id text,
#     relation text
# ) SERVER openfga_server;

# -- Grant permissions
# GRANT SELECT ON acl_permission TO PUBLIC;

# -- Set GUC parameters
# -- SET postfga.endpoint = 'dns:///openfga:8081';
# -- SET postfga.store_id = 'test-store';
# -- SET postfga.cache_ttl_ms = 60000;

