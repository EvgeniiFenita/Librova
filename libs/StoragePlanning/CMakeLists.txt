add_library(LibriFlowStoragePlanning STATIC
    ManagedLibraryLayout.cpp
)

target_include_directories(LibriFlowStoragePlanning PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibriFlowStoragePlanning PUBLIC
    LibriFlowDomain
)

target_compile_features(LibriFlowStoragePlanning PUBLIC cxx_std_20)
