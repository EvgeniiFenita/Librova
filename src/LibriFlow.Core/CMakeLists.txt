add_library(LibriFlowCore STATIC
    src/Version.cpp
)

target_include_directories(LibriFlowCore
    PUBLIC
        include
)

target_compile_features(LibriFlowCore PUBLIC cxx_std_20)
