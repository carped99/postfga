# Fetch standalone ASIO (header-only)
FetchContent_Declare(asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-36-0
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(asio)

# FetchContent_GetProperties(asio)
# if(NOT asio_POPULATED)
#     FetchContent_Populate(asio)
# endif()

# Set ASIO include directory
set(ASIO_INCLUDE_DIR ${asio_SOURCE_DIR}/asio/include)

# # Export variables for parent CMakeLists
# set(OPENFGA_PROTO_INCLUDE_DIR ${openfga_proto_SOURCE_DIR} PARENT_SCOPE)
# set(OPENFGA_GRPC_INCLUDE_DIR ${openfga_grpc_SOURCE_DIR} PARENT_SCOPE)
# set(ASIO_INCLUDE_DIR ${ASIO_INCLUDE_DIR} PARENT_SCOPE)

# # Create interface library for OpenFGA
# add_library(openfga_api INTERFACE)
# target_include_directories(openfga_api INTERFACE
#     ${openfga_proto_SOURCE_DIR}
#     ${openfga_grpc_SOURCE_DIR}
# )
# target_link_libraries(openfga_api INTERFACE
#     openfga_proto
#     openfga_grpc
# )