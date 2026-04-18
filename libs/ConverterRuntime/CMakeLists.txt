add_library(LibrovaConverterRuntime STATIC
    ExternalBookConverter.cpp
)

target_include_directories(LibrovaConverterRuntime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibrovaConverterRuntime PUBLIC
    LibrovaConverterCommand
    LibrovaDomain
    LibrovaFoundation
)

target_compile_features(LibrovaConverterRuntime PUBLIC cxx_std_20)
