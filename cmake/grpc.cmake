cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(MSVC)
  add_definitions(-D_WIN32_WINNT=0x600)
endif()

find_package(Threads REQUIRED)

message(STATUS "Using gRPC via FetchContent")

set(gRPC_BUILD_TESTS OFF CACHE BOOL "Disable gRPC tests" FORCE)
set(protobuf_INSTALL OFF CACHE BOOL "Disable protobuf install" FORCE)
set(protobuf_BUILD_TESTS OFF CACHE BOOL "Disable protobuf tests" FORCE)
set(ABSL_ENABLE_INSTALL ON CACHE BOOL "" FORCE)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  gRPC
  GIT_REPOSITORY https://github.com/grpc/grpc.git
  GIT_TAG        v1.76.0
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   TRUE
)

FetchContent_MakeAvailable(gRPC)

message(STATUS "gRPC source dir: ${gRPC_SOURCE_DIR}")
message(STATUS "gRPC binary dir: ${gRPC_BINARY_DIR}")

# 테스트/컨포먼스 빌드는 끔 (속도 + 의존성 절약)
#set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
#set(protobuf_BUILD_CONFORMANCE OFF CACHE BOOL "" FORCE)
#set(gRPC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
