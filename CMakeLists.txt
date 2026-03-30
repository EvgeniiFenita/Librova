cmake_minimum_required(VERSION 3.26)

project(LibriFlow VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_subdirectory(libs/Domain)
add_subdirectory(libs/Core)
add_subdirectory(libs/StoragePlanning)
add_subdirectory(tests/Unit)
