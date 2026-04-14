find_package(Stb REQUIRED)

add_library(LibrovaCoverProcessingStb STATIC
    StbCoverImageProcessor.cpp
    StbCoverImageProcessor.hpp
)

target_include_directories(LibrovaCoverProcessingStb
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
    PRIVATE
        ${Stb_INCLUDE_DIR}
)

target_link_libraries(LibrovaCoverProcessingStb
    PUBLIC
        LibrovaDomain
)

target_compile_features(LibrovaCoverProcessingStb PUBLIC cxx_std_20)
