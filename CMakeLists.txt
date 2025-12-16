cmake_minimum_required(VERSION 3.14)

project(postfga LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

include(cmake/openfga.cmake)
include(cmake/postgresql.cmake)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS
        src/*.c src/*.cpp
)

add_library(postfga MODULE ${SRCS})

set_target_properties(postfga PROPERTIES
        PREFIX ""
        OUTPUT_NAME postfga
        POSITION_INDEPENDENT_CODE ON
)

target_include_directories(postfga PRIVATE
        ${PostgreSQL_INCLUDE_DIRS}
        ${PostgreSQL_TYPE_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(postfga PRIVATE openfga)

# extension library(.so)
install(TARGETS postfga
        LIBRARY DESTINATION ${PG_PKGLIBDIR}
)

# extension files(control, sql)
file(GLOB POSTFGA_EXTENSION_FILES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/sql/*.sql"
        "${CMAKE_SOURCE_DIR}/*.control"
)

install(FILES ${POSTFGA_EXTENSION_FILES}
        DESTINATION ${PG_SHAREDIR}/extension
)
