include(CTest)

if(BUILD_TESTING)
    find_package(Catch2 CONFIG REQUIRED)

    add_executable(LibriFlowCoreTests
        VersionTests.cpp
    )

    target_link_libraries(LibriFlowCoreTests
        PRIVATE
            Catch2::Catch2WithMain
            LibriFlowCore
    )

    target_compile_features(LibriFlowCoreTests PRIVATE cxx_std_20)

    include(Catch)
    catch_discover_tests(LibriFlowCoreTests)
endif()
