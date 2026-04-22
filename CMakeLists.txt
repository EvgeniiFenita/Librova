cmake_minimum_required(VERSION 3.26)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION.txt" LIBROVA_PRODUCT_VERSION LIMIT_COUNT 1)
string(STRIP "${LIBROVA_PRODUCT_VERSION}" LIBROVA_PRODUCT_VERSION)

project(Librova VERSION ${LIBROVA_PRODUCT_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)

    if(DEFINED VCPKG_TARGET_TRIPLET AND VCPKG_TARGET_TRIPLET MATCHES "-static$")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

include(CTest)

add_subdirectory(libs/Domain)
add_subdirectory(libs/Foundation)
add_subdirectory(libs/Database)
add_subdirectory(libs/Converter)
add_subdirectory(libs/Storage)
add_subdirectory(libs/Parsing)
add_subdirectory(libs/Import)
add_subdirectory(libs/App)
add_subdirectory(libs/ApplicationClient)
add_subdirectory(libs/Transport)
add_subdirectory(libs/Rpc)

add_subdirectory(apps/Librova.Core.Host)
add_subdirectory(tests/Unit)
