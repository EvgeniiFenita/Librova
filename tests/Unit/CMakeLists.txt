include(CTest)

if(BUILD_TESTING)
    find_package(Catch2 CONFIG REQUIRED)

    add_executable(LibriFlowCoreTests
        TestBook.cpp
        TestBookRepository.cpp
        TestBookFormat.cpp
        TestBookId.cpp
        TestDatabaseSchema.cpp
        TestMetadataNormalization.cpp
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
            LibriFlowDomain
            LibriFlowCore
            LibriFlowDatabaseSchema
            LibriFlowSqlite
            LibriFlowStoragePlanning
    )

    target_compile_features(LibriFlowCoreTests PRIVATE cxx_std_20)
    add_test(NAME LibriFlowCoreTests COMMAND LibriFlowCoreTests)
endif()
