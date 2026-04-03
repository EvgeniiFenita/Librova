add_library(LibrovaCoverProcessingWic STATIC
    WicCoverImageProcessor.cpp
    WicCoverImageProcessor.hpp
)

target_include_directories(LibrovaCoverProcessingWic
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaCoverProcessingWic
    PUBLIC
        LibrovaDomain
)

if(WIN32)
    target_link_libraries(LibrovaCoverProcessingWic
        PUBLIC
            ole32
            windowscodecs
    )
endif()

target_compile_features(LibrovaCoverProcessingWic PUBLIC cxx_std_20)
