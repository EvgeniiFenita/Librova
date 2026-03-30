add_library(LibriFlowManagedStorage STATIC
    ManagedFileStorage.cpp
)

target_include_directories(LibriFlowManagedStorage PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(LibriFlowManagedStorage PUBLIC
    LibriFlowDomain
    LibriFlowStoragePlanning
)

target_compile_features(LibriFlowManagedStorage PUBLIC cxx_std_20)
