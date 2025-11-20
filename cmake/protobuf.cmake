set(FETCHCONTENT_QUIET OFF)

# ===== Protobuf v33.0.0 =====
set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_CONFORMANCE OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  protobuf
  GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
  GIT_TAG        v33.1
  GIT_SHALLOW    TRUE
  EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(protobuf)