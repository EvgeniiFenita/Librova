include(CTest)

if(BUILD_TESTING)
    find_package(Catch2 CONFIG REQUIRED)
    find_package(Stb REQUIRED)
    include(Catch)

    add_executable(LibrovaCoreTests
        TestHashing.cpp
        TestImportJobClient.cpp
        TestImportJobService.cpp
        TestImportJobProtoMapper.cpp
        TestImportDiagnosticText.cpp
        TestImportSourceExpander.cpp
        TestLibraryCatalogProtoMapper.cpp
        TestLibraryJobServiceAdapter.cpp
        TestNamedPipeClient.cpp
        TestNamedPipeChannel.cpp
        TestNamedPipeHost.cpp
        TestPipeProtocol.cpp
        TestPipeRequestDispatcher.cpp
        TestProtoContracts.cpp
        TestLibraryCatalogFacade.cpp
        TestLibraryExportFacade.cpp
        TestLibraryTrashFacade.cpp
        TestLibraryImportFacade.cpp
        TestBook.cpp
        TestSqliteBookRepository.cpp
        TestBookRepository.cpp
        TestConverterCommandBuilder.cpp
        TestConverterConfiguration.cpp
        TestExternalBookConverter.cpp
        TestEpubParser.cpp
        TestFb2GenreMapper.cpp
        TestFb2Parser.cpp
        TestCoreHostApp.cpp
        TestHostOptions.cpp
        TestLibraryBootstrap.cpp
        TestBookFormat.cpp
        TestBookId.cpp
        TestImportConversionPolicy.cpp
        TestImportPerfTracker.cpp
        TestImportJobManager.cpp
        TestImportJobRunner.cpp
        TestImportRollbackService.cpp
        TestLogging.cpp
        TestUnicodeConversion.cpp
        TestSingleFileImportCoordinator.cpp
        TestSchemaMigrator.cpp
        TestDatabaseSchema.cpp
        TestManagedFileStorage.cpp
        TestManagedFileEncoding.cpp
        TestManagedTrashService.cpp
        TestManagedPathSafety.cpp
        TestMetadataNormalization.cpp
        TestParserRegistry.cpp
        TestSearchQuery.cpp
        TestServiceContracts.cpp
        TestManagedLibraryLayout.cpp
        TestSqliteConnection.cpp
        TestDomainError.cpp
        TestDuplicateMatch.cpp
        TestVersion.cpp
        TestStbCoverImageProcessor.cpp
        TestWindowsRecycleBinService.cpp
        TestWriterDispatchingRepository.cpp
        TestZipImportCoordinator.cpp
    )

    target_link_libraries(LibrovaCoreTests
        PRIVATE
            Catch2::Catch2WithMain
            LibrovaApplication
            LibrovaApplicationClient
            LibrovaApplicationJobs
            LibrovaBookDatabase
            LibrovaConverter
            LibrovaCoverProcessingStb
            LibrovaDomain
            LibrovaFoundation
            LibrovaCoreHost
            LibrovaDatabaseRuntime
            LibrovaDatabaseSchema
            LibrovaParsing
            LibrovaImportConversion
            LibrovaImporting
            LibrovaJobs
            LibrovaManagedStorage
            LibrovaManagedFileEncoding
            LibrovaManagedPaths
            LibrovaManagedTrash
            LibrovaPipeClient
            LibrovaPipeHost
            LibrovaPipeTransport
            LibrovaProtoContracts
            LibrovaProtoMapping
            LibrovaProtoServices
            LibrovaRecycleBin
            LibrovaSearchIndex
            LibrovaSqlite
            LibrovaStoragePlanning
            LibrovaZipImporting
    )

    target_compile_features(LibrovaCoreTests PRIVATE cxx_std_20)
    target_include_directories(LibrovaCoreTests PRIVATE ${Stb_INCLUDE_DIR})
    target_compile_definitions(LibrovaCoreTests PRIVATE
        LIBROVA_CORE_HOST_APP_NAME="LibrovaCoreHostApp.exe"
        LIBROVA_TEST_CONFIGURATION="$<CONFIG>"
    )
    if(MSVC)
        target_compile_options(LibrovaCoreTests PRIVATE /FS)
    endif()

    catch_discover_tests(
        LibrovaCoreTests
        DISCOVERY_MODE POST_BUILD
        DISCOVERY_TIMEOUT 60
    )
endif()

