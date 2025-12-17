#!/bin/bash
set -euo pipefail


validate_database_name() {
    local name=$1
    [[ "$name" =~ ^[a-zA-Z_][a-zA-Z0-9_]{0,62}$ ]]
}

create_database() {
    local db="$1"
    local password="${2:-${POSTGRES_PASSWORD}}"

    if ! validate_database_name "$db"; then
        echo "ERROR: Invalid database name: $db" >&2
        return 1
    fi

    echo "Create database: $db"

    "${psql[@]}" -q -v username="$db" -v password="$password" <<-'EOSQL'
	SELECT format('CREATE ROLE %I LOGIN PASSWORD %L', :'username', :'password')
	WHERE NOT EXISTS (SELECT 1 FROM pg_roles WHERE rolname = :'username');
	\gexec
EOSQL

    "${psql[@]}" -q -v dbname="$db" -v username="$db" <<-'EOSQL'
    SELECT FORMAT('CREATE DATABASE %I OWNER %I', :'dbname', :'username')
	WHERE NOT EXISTS (SELECT 1 FROM pg_database WHERE datname = :'dbname');
	\gexec
EOSQL
}

if [[ -n "${POSTGRES_DATABASES:-}" ]]; then
    IFS=',' read -ra DBS <<< "$POSTGRES_DATABASES"
    for db in "${DBS[@]}"; do
        db="${db//[[:space:]]/}"   # 공백 제거
        [[ -z "$db" ]] && continue
        create_database "$db"
    done
fi