find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

message(STATUS "Protobuf include dirs = ${Protobuf_INCLUDE_DIRS}")
message(STATUS "Protobuf include dir  = ${Protobuf_INCLUDE_DIR}")

file(GLOB_RECURSE OPENFGA_SRCS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/api/*.cc
)

add_library(openfga STATIC
    ${OPENFGA_SRCS}
)

# Enable -fPIC for shared library linking
set_target_properties(openfga PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(openfga
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/api
)

# protobuf 런타임에 링크
target_link_libraries(openfga
    PUBLIC
        protobuf::libprotobuf        # Debian / CMake 설정에 맞게 조정
        grpc++                       # 또는 gRPC::grpc++ (설치 방식에 따라)
        Threads::Threads
)

