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
        LibrovaCoreHost
        LibrovaDatabaseRuntime
        LibrovaImporting
        LibrovaJobs
        LibrovaLogging
        LibrovaManagedStorage
        LibrovaManagedTrash
        LibrovaParserRegistry
        LibrovaPipeHost
        LibrovaProtoServices
        LibrovaProtoMapping
        LibrovaStoragePlanning
        LibrovaZipImporting
)

target_compile_features(LibrovaCoreHostApp PRIVATE cxx_std_20)
