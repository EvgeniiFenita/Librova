add_executable(LibrovaCoreHostApp
    Main.cpp
)

target_include_directories(LibrovaCoreHostApp
    PRIVATE
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaCoreHostApp
    PRIVATE
        LibrovaApplication
        LibrovaApplicationJobs
        LibrovaBookDatabase
        LibrovaConverterConfiguration
        LibrovaConverterRuntime
        LibrovaCoverProcessingWic
        LibrovaCore
        LibrovaCoreHost
        LibrovaDatabaseRuntime
        LibrovaImporting
        LibrovaJobs
        LibrovaLogging
        LibrovaManagedPaths
        LibrovaManagedStorage
        LibrovaManagedTrash
        LibrovaParserRegistry
        LibrovaPipeHost
        LibrovaProtoServices
        LibrovaProtoMapping
        LibrovaStoragePlanning
        LibrovaUnicode
        LibrovaZipImporting
        LibrovaRecycleBin
)

target_compile_features(LibrovaCoreHostApp PRIVATE cxx_std_20)
