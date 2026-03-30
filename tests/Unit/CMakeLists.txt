include(CTest)

if(BUILD_TESTING)
    find_package(Catch2 CONFIG REQUIRED)

    add_executable(LibriFlowCoreTests
        TestImportJobService.cpp
        TestImportJobProtoMapper.cpp
        TestLibraryJobServiceAdapter.cpp
        TestProtoContracts.cpp
        TestLibraryImportFacade.cpp
        TestBook.cpp
        TestSqliteBookRepository.cpp
        TestBookRepository.cpp
        TestConverterCommandBuilder.cpp
        TestConverterConfiguration.cpp
        TestExternalBookConverter.cpp
        TestEpubParser.cpp
        TestFb2Parser.cpp
        TestBookFormat.cpp
        TestBookId.cpp
        TestImportConversionPolicy.cpp
        TestImportJobManager.cpp
        TestImportJobRunner.cpp
        TestSingleFileImportCoordinator.cpp
        TestSchemaMigrator.cpp
        TestDatabaseSchema.cpp
        TestManagedFileStorage.cpp
        TestMetadataNormalization.cpp
        TestParserRegistry.cpp
        TestSearchQuery.cpp
        TestServiceContracts.cpp
        TestManagedLibraryLayout.cpp
        TestSqliteConnection.cpp
        TestDomainError.cpp
        TestDuplicateMatch.cpp
        TestVersion.cpp
        TestZipImportCoordinator.cpp
    )

    target_link_libraries(LibriFlowCoreTests
        PRIVATE
            Catch2::Catch2WithMain
            LibriFlowApplication
            LibriFlowApplicationJobs
            LibriFlowBookDatabase
            LibriFlowConverterCommand
            LibriFlowConverterConfiguration
            LibriFlowConverterRuntime
            LibriFlowDomain
            LibriFlowCore
            LibriFlowDatabaseRuntime
            LibriFlowDatabaseSchema
            LibriFlowEpubParsing
            LibriFlowFb2Parsing
            LibriFlowImportConversion
            LibriFlowImporting
            LibriFlowJobs
            LibriFlowManagedStorage
            LibriFlowParserRegistry
            LibriFlowProtoContracts
            LibriFlowProtoMapping
            LibriFlowProtoServices
            LibriFlowSearchIndex
            LibriFlowSqlite
            LibriFlowStoragePlanning
            LibriFlowZipImporting
    )

    target_compile_features(LibriFlowCoreTests PRIVATE cxx_std_20)
    add_test(NAME LibriFlowCoreTests COMMAND LibriFlowCoreTests)
endif()
