add_library(LibrovaRecycleBin STATIC
    WindowsRecycleBinService.cpp
    WindowsRecycleBinService.hpp
)

target_include_directories(LibrovaRecycleBin PUBLIC
    ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaRecycleBin PUBLIC
    LibrovaDomain
    LibrovaFoundation
)

target_link_libraries(LibrovaRecycleBin PRIVATE
    shell32
)

target_compile_features(LibrovaRecycleBin PUBLIC cxx_std_20)
