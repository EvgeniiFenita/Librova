find_package(protobuf CONFIG REQUIRED)

set(LIBRIFLOW_PROTO_SOURCE_DIR ${CMAKE_SOURCE_DIR}/proto)
set(LIBRIFLOW_PROTO_BINARY_DIR ${CMAKE_BINARY_DIR}/generated/proto)
set(LIBRIFLOW_PROTO_FILE ${LIBRIFLOW_PROTO_SOURCE_DIR}/import_jobs.proto)
set(LIBRIFLOW_PROTO_CC ${LIBRIFLOW_PROTO_BINARY_DIR}/import_jobs.pb.cc)
set(LIBRIFLOW_PROTO_H ${LIBRIFLOW_PROTO_BINARY_DIR}/import_jobs.pb.h)

file(MAKE_DIRECTORY ${LIBRIFLOW_PROTO_BINARY_DIR})

find_program(LIBRIFLOW_PROTOC_EXECUTABLE
    NAMES protoc protoc.exe
    PATHS
        ${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/tools/protobuf
        ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/tools/protobuf
    NO_DEFAULT_PATH
    REQUIRED
)

add_custom_command(
    OUTPUT
        ${LIBRIFLOW_PROTO_CC}
        ${LIBRIFLOW_PROTO_H}
    COMMAND ${LIBRIFLOW_PROTOC_EXECUTABLE}
        --proto_path=${LIBRIFLOW_PROTO_SOURCE_DIR}
        --cpp_out=${LIBRIFLOW_PROTO_BINARY_DIR}
        ${LIBRIFLOW_PROTO_FILE}
    DEPENDS
        ${LIBRIFLOW_PROTO_FILE}
    COMMENT "Generating C++ protobuf sources for import_jobs.proto"
    VERBATIM
)

add_library(LibriFlowProtoContracts STATIC
    ${LIBRIFLOW_PROTO_CC}
    ${LIBRIFLOW_PROTO_H}
)

target_include_directories(LibriFlowProtoContracts
    PUBLIC
        ${LIBRIFLOW_PROTO_BINARY_DIR}
)

target_link_libraries(LibriFlowProtoContracts
    PUBLIC
        protobuf::libprotobuf
)

target_compile_features(LibriFlowProtoContracts PUBLIC cxx_std_20)
