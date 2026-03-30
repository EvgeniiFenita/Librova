add_executable(LibriFlowCoreHostApp
    Main.cpp
)

target_include_directories(LibriFlowCoreHostApp
    PRIVATE
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibriFlowCoreHostApp
    PRIVATE
        LibriFlowApplication
        LibriFlowApplicationJobs
        LibriFlowBookDatabase
        LibriFlowConverterConfiguration
        LibriFlowConverterRuntime
        LibriFlowCoreHost
        LibriFlowDatabaseRuntime
        LibriFlowImporting
        LibriFlowJobs
        LibriFlowManagedStorage
        LibriFlowParserRegistry
        LibriFlowPipeHost
        LibriFlowProtoServices
        LibriFlowProtoMapping
        LibriFlowStoragePlanning
        LibriFlowZipImporting
)

target_compile_features(LibriFlowCoreHostApp PRIVATE cxx_std_20)
