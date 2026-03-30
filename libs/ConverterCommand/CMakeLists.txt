add_library(LibrovaConverterCommand STATIC
    ConverterCommandBuilder.cpp
)

target_include_directories(LibrovaConverterCommand PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibrovaConverterCommand PUBLIC
    LibrovaDomain
)

target_compile_features(LibrovaConverterCommand PUBLIC cxx_std_20)
