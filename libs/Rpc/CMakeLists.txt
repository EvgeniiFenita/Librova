find_package(protobuf CONFIG REQUIRED)

set(LIBROVA_PROTO_SOURCE_DIR ${CMAKE_SOURCE_DIR}/proto)
set(LIBROVA_PROTO_BINARY_DIR ${CMAKE_BINARY_DIR}/generated/proto)
set(LIBROVA_PROTO_FILE ${LIBROVA_PROTO_SOURCE_DIR}/import_jobs.proto)
set(LIBROVA_PROTO_CC ${LIBROVA_PROTO_BINARY_DIR}/import_jobs.pb.cc)
set(LIBROVA_PROTO_H ${LIBROVA_PROTO_BINARY_DIR}/import_jobs.pb.h)

file(MAKE_DIRECTORY ${LIBROVA_PROTO_BINARY_DIR})

find_program(LIBROVA_PROTOC_EXECUTABLE
    NAMES protoc protoc.exe
    PATHS
        ${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/tools/protobuf
        ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/tools/protobuf
    NO_DEFAULT_PATH
    REQUIRED
)

add_custom_command(
    OUTPUT
        ${LIBROVA_PROTO_CC}
        ${LIBROVA_PROTO_H}
    COMMAND ${LIBROVA_PROTOC_EXECUTABLE}
        --proto_path=${LIBROVA_PROTO_SOURCE_DIR}
        --cpp_out=${LIBROVA_PROTO_BINARY_DIR}
        ${LIBROVA_PROTO_FILE}
    DEPENDS
        ${LIBROVA_PROTO_FILE}
    COMMENT "Generating C++ protobuf sources for import_jobs.proto"
    VERBATIM
)

add_library(LibrovaRpc STATIC
    ${LIBROVA_PROTO_CC}
    ImportJobProtoMapper.cpp
    LibraryCatalogProtoMapper.cpp
    LibraryJobServiceAdapter.cpp
)

target_include_directories(LibrovaRpc
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        ${LIBROVA_PROTO_BINARY_DIR}
)

target_link_libraries(LibrovaRpc
    PUBLIC
        LibrovaApplication
        LibrovaApplicationJobs
        LibrovaDomain
        protobuf::libprotobuf
    PRIVATE
        LibrovaFoundation
)

target_compile_features(LibrovaRpc PUBLIC cxx_std_20)
