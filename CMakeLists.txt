cmake_minimum_required(VERSION 3.20)
project(postfga VERSION 1.0.0 LANGUAGES C CXX)

# C++20 standard required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
include(cmake/postgresql.cmake)
include(cmake/asio.cmake)
# include(cmake/grpc.cmake)
# include(cmake/protobuf.cmake)
include(cmake/openfga.cmake)

# Extension source files
set(SOURCES
    src/init.c
    src/bgw.c
    src/cache.c
    src/generation.c
    src/guc.c
    src/relation.c
    src/state.c
    src/stats.c
    src/grpc_client.cpp
)

# Create PostgreSQL extension shared library (MODULE → .so)
add_library(postfga MODULE ${SOURCES})

# Include directories
target_include_directories(postfga PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_include_directories(postfga SYSTEM PRIVATE
    ${PG_INCLUDE_SERVER}
    ${PG_INCLUDE}
    ${Protobuf_INCLUDE_DIRS}
    ${ASIO_INCLUDE_DIR}
)

# Compiler definitions for standalone ASIO
target_compile_definitions(postfga PRIVATE
    ASIO_STANDALONE
    ASIO_NO_DEPRECATED
)

# # Link libraries
target_link_libraries(postfga PRIVATE
    openfga
)

# Compiler flags
target_compile_options(postfga PRIVATE
    -Wall
    -Wextra
    -Werror=unused-variable
    -fPIC
)

# Set output name (no lib prefix for PostgreSQL extensions)
set_target_properties(postfga PROPERTIES
    PREFIX ""          # lib 접두어 제거
    OUTPUT_NAME "postfga"
    SUFFIX ".so"
)

# Installation paths
set(EXTENSION_INSTALL_DIR "${PG_PKGLIBDIR}")
set(EXTENSION_SHARE_DIR "${PG_SHAREDIR}/extension")

# Install extension library
install(TARGETS postfga
    LIBRARY DESTINATION ${EXTENSION_INSTALL_DIR}
)

# Install SQL and control files
install(FILES
    sql/postfga.control
    sql/postfga--1.0.0.sql
    DESTINATION ${EXTENSION_SHARE_DIR}
)

# Development target: print configuration
add_custom_target(show-config
    COMMAND ${CMAKE_COMMAND} -E echo "PostgreSQL config:"
    COMMAND ${CMAKE_COMMAND} -E echo "  Include: ${PG_INCLUDE_SERVER}"
    COMMAND ${CMAKE_COMMAND} -E echo "  Library: ${PG_PKGLIBDIR}"
    COMMAND ${CMAKE_COMMAND} -E echo "  Share: ${PG_SHAREDIR}"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Extension will be installed to:"
    COMMAND ${CMAKE_COMMAND} -E echo "  Library: ${EXTENSION_INSTALL_DIR}/postfga.so"
    COMMAND ${CMAKE_COMMAND} -E echo "  Control: ${EXTENSION_SHARE_DIR}/postfga.control"
    COMMAND ${CMAKE_COMMAND} -E echo "  SQL: ${EXTENSION_SHARE_DIR}/postfga--1.0.0.sql"
)

# Uninstall target
add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -E remove ${EXTENSION_INSTALL_DIR}/postfga.so
    COMMAND ${CMAKE_COMMAND} -E remove ${EXTENSION_SHARE_DIR}/postfga.control
    COMMAND ${CMAKE_COMMAND} -E remove ${EXTENSION_SHARE_DIR}/postfga--1.0.0.sql
    COMMENT "Uninstalling postfga extension"
)

# Test executable for gRPC client (optional)
option(BUILD_GRPC_TEST "Build gRPC client test program" OFF)

if(BUILD_GRPC_TEST)
    add_executable(test_grpc_client
        tests/test_grpc_client.c
        src/grpc_client.cpp
    )

    target_include_directories(test_grpc_client PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${OPENFGA_PROTO_INCLUDE_DIR}
        ${OPENFGA_GRPC_INCLUDE_DIR}
        ${ASIO_INCLUDE_DIR}
    )

    target_compile_definitions(test_grpc_client PRIVATE
        ASIO_STANDALONE
        ASIO_NO_DEPRECATED
    )

    target_link_libraries(test_grpc_client PRIVATE
        openfga_api
        gRPC::grpc++
        protobuf::libprotobuf
        pthread
    )

    install(TARGETS test_grpc_client
        RUNTIME DESTINATION bin
    )
endif()
