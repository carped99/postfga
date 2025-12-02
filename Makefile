# --------------------------------------------------------------
# PostgreSQL Extension Configuration
# --------------------------------------------------------------
MODULE_big = postfga
EXTENSION  = postfga
CONTROL    = postfga.control
DATA       = sql/postfga--1.0.0.sql

# --------------------------------------------------------------
# Source files
# --------------------------------------------------------------
SRCS := $(shell find api src -name '*.c' -o -name '*.cc' -o -name '*.cpp')

# Convert source files to object files
OBJS := $(addsuffix .o,$(basename $(SRCS)))

# --------------------------------------------------------------
# Compile flags
# --------------------------------------------------------------
PG_CPPFLAGS += -I./src -I./api -Ithird_party/asio/asio/include
PG_CXXFLAGS += -std=c++20 -Wall -Wextra \
				-DASIO_STANDALONE \
				-DASIO_NO_DEPRECATED

# --------------------------------------------------------------
# Link flags
# --------------------------------------------------------------
SHLIB_LINK += -lgrpc++ -lgrpc -lprotobuf -lpthread -lxxhash

# --------------------------------------------------------------
# Build type (debug / release)
# --------------------------------------------------------------
BUILD ?= debug

ifeq ($(BUILD),debug)
  $(info [postfga] Build type: DEBUG)
  override PG_CFLAGS   += -O0 -g3 -DDEBUG
  override PG_CXXFLAGS += -O0 -g3 -DDEBUG
else  # release
  $(info [postfga] Build type: RELEASE)
  override PG_CFLAGS   += -g0 -O3 -DNDEBUG
  override PG_CXXFLAGS += -g0 -O3 -DNDEBUG
endif

# --------------------------------------------------------------
# PGXS
# --------------------------------------------------------------
override with_llvm = no
override enable_debug = no
override enable_dtrace = no

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

# --------------------------------------------------------------
# Utility targets
# --------------------------------------------------------------
.PHONY: debug release bear up start stop drop create reload

# Build with debug symbols
debug:
	$(MAKE) BUILD=debug

# Build with release
release:
	$(MAKE) BUILD=release

# Generate compile_commands.json (for VSCode/clangd)
bear: clean
	bear -- make -j12

up: install
	su - postgres -c "postgres -c shared_preload_libraries=postfga"

# Start PostgreSQL
start: install
	su - postgres -c "pg_ctl -o \"-c shared_preload_libraries=postfga\" start" 

# Stop PostgreSQL
stop:
	su - postgres -c "pg_ctl stop"

# Drop extension
drop:
	su - postgres -c "psql -c \"DROP EXTENSION IF EXISTS postfga CASCADE;\""

# Create extension
create:
	su - postgres -c "psql -c \"CREATE EXTENSION IF NOT EXISTS postfga;\""

# Reload extension
reload: drop create