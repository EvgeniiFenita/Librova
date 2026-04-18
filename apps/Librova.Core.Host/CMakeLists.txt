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
        LibrovaDatabase
        LibrovaConverter
        LibrovaCoverProcessingStb
        LibrovaCoreHost
        LibrovaFoundation
        LibrovaImporting
        LibrovaJobs
        LibrovaManagedPaths
        LibrovaManagedStorage
        LibrovaManagedTrash
        LibrovaParsing
        LibrovaPipeHost
        LibrovaProtoServices
        LibrovaProtoMapping
        LibrovaStoragePlanning
        LibrovaZipImporting
        LibrovaRecycleBin
)

target_compile_features(LibrovaCoreHostApp PRIVATE cxx_std_20)
