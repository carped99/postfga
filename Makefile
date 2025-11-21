# --------------------------------------------------------------
# PostgreSQL Extension Configuration
# --------------------------------------------------------------
MODULE_big = postfga
EXTENSION  = postfga
DATA       = sql/postfga--1.0.0.sql
CONTROL    = sql/postfga.control

# --------------------------------------------------------------
# Source files
# --------------------------------------------------------------
SRCS := $(shell find api src -name '*.c' -o -name '*.cc' -o -name '*.cpp')

# Convert source files to object files
OBJS := $(SRCS:.c=.o)
OBJS := $(OBJS:.cpp=.o)
OBJS := $(OBJS:.cc=.o)

# --------------------------------------------------------------
# Compile flags
# --------------------------------------------------------------
PG_CPPFLAGS += -Isrc -Iapi
PG_CXXFLAGS += -std=c++17 -Wall -Wextra

# --------------------------------------------------------------
# Link flags
# --------------------------------------------------------------
SHLIB_LINK += -lgrpc++ -lgrpc -lprotobuf -lpthread

# --------------------------------------------------------------
# PGXS
# --------------------------------------------------------------
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

# --------------------------------------------------------------
# Utility targets
# --------------------------------------------------------------
.PHONY: bear up start stop drop create reload

# Generate compile_commands.json (for VSCode/clangd)
bear:
	bear -- make

up:
	su - postgres -c "postgres -c shared_preload_libraries=postfga"

# Start PostgreSQL
start:
	su - postgres -c "pg_ctl -o \"-c shared_preload_libraries=postfga\" start" 

# Stop PostgreSQL
stop:
	su - postgres -c "pg_ctl stop"

# Drop extension
drop:
	psql -c "DROP EXTENSION IF EXISTS postfga CASCADE;"

# Create extension
create:
	psql -c "CREATE EXTENSION IF NOT EXISTS postfga;"

# Reload extension
reload: drop create