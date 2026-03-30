add_library(LibrovaPipeHost STATIC
    NamedPipeHost.cpp
    NamedPipeHost.hpp
)

target_include_directories(LibrovaPipeHost
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaPipeHost
    PUBLIC
        LibrovaPipeTransport
)

target_compile_features(LibrovaPipeHost PUBLIC cxx_std_20)
