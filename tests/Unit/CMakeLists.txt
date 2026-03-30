include(CTest)

if(BUILD_TESTING)
    find_package(Catch2 CONFIG REQUIRED)

    add_executable(LibriFlowCoreTests
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
    )

    target_link_libraries(LibriFlowCoreTests
        PRIVATE
            Catch2::Catch2WithMain
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
            LibriFlowManagedStorage
            LibriFlowParserRegistry
            LibriFlowSearchIndex
            LibriFlowSqlite
            LibriFlowStoragePlanning
    )

    target_compile_features(LibriFlowCoreTests PRIVATE cxx_std_20)
    add_test(NAME LibriFlowCoreTests COMMAND LibriFlowCoreTests)
endif()
