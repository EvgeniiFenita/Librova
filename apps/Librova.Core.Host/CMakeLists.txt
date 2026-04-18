add_executable(LibrovaCoreHostApp
    Main.cpp
)

target_include_directories(LibrovaCoreHostApp
    PRIVATE
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaCoreHostApp
    PRIVATE
        LibrovaApp
        LibrovaDatabase
        LibrovaParsing
        LibrovaTransport
        LibrovaRpc
)

target_compile_features(LibrovaCoreHostApp PRIVATE cxx_std_20)
