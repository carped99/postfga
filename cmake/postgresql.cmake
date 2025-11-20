cmake_minimum_required(VERSION 3.22)

find_package(PostgreSQL REQUIRED)

# # Find PostgreSQL
find_program(PG_CONFIG pg_config REQUIRED)

# # Get PostgreSQL configuration
execute_process(COMMAND ${PG_CONFIG} --includedir-server
                OUTPUT_VARIABLE PG_INCLUDE_SERVER
                OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${PG_CONFIG} --includedir
                OUTPUT_VARIABLE PG_INCLUDE
                OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${PG_CONFIG} --pkglibdir
                OUTPUT_VARIABLE PG_PKGLIBDIR
                OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${PG_CONFIG} --sharedir
                OUTPUT_VARIABLE PG_SHAREDIR
                OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${PG_CONFIG} --version
                OUTPUT_VARIABLE PG_VERSION_STRING
                OUTPUT_STRIP_TRAILING_WHITESPACE)

message(STATUS "PostgreSQL version (from CMake): ${PostgreSQL_VERSION}")
message(STATUS "PostgreSQL include dirs: ${PostgreSQL_INCLUDE_DIRS}")
message(STATUS "PostgreSQL libdir: ${PostgreSQL_LIBRARY_DIRS}")


# message(STATUS "PostgreSQL version: ${PG_VERSION_STRING}")
# message(STATUS "PostgreSQL include (server): ${PG_INCLUDE_SERVER}")
# message(STATUS "PostgreSQL pkglibdir: ${PG_PKGLIBDIR}")
# message(STATUS "PostgreSQL sharedir: ${PG_SHAREDIR}")
