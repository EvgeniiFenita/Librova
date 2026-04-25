cmake_minimum_required(VERSION 3.26)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Test)

qt_standard_project_setup(REQUIRES 6.5)

set(LIBROVA_QT_APP_DIR "${CMAKE_SOURCE_DIR}/apps/Librova.Qt")

# Resolve the Qt bin directory from the Qt6::Core target so tests find Qt DLLs
# without requiring the Qt bin dir to be on the system PATH.
get_target_property(_qt_core_loc Qt6::Core IMPORTED_LOCATION_DEBUG)
if(NOT _qt_core_loc)
    get_target_property(_qt_core_loc Qt6::Core IMPORTED_LOCATION_RELEASE)
endif()
if(NOT _qt_core_loc)
    get_target_property(_qt_core_loc Qt6::Core IMPORTED_LOCATION)
endif()
get_filename_component(LIBROVA_QT_BIN_DIR "${_qt_core_loc}" DIRECTORY)

# Shared helper: register a test target and inject Qt bin dir into PATH.
function(_librova_qt_register_test TARGET_NAME)
    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
    set_tests_properties(${TARGET_NAME} PROPERTIES
        ENVIRONMENT "PATH=${LIBROVA_QT_BIN_DIR};$ENV{PATH}"
    )
endfunction()

function(add_qt_test TARGET_NAME SOURCE_FILE)
    qt_add_executable(${TARGET_NAME} ${SOURCE_FILE})

    target_include_directories(${TARGET_NAME} PRIVATE
        ${LIBROVA_QT_APP_DIR}
    )

    target_link_libraries(${TARGET_NAME}
        PRIVATE
            Qt6::Core
            Qt6::Test
            LibrovaApp
            LibrovaFoundation
    )

    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)
    target_compile_definitions(${TARGET_NAME} PRIVATE
        LIBROVA_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )

    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /utf-8 /FS)
    endif()

    _librova_qt_register_test(${TARGET_NAME})
endfunction()

# Pull in the production source files that tests depend on.
# Each test links against a small set of app sources rather than a library target.
function(add_qt_test_with_app_sources TARGET_NAME SOURCE_FILE)
    qt_add_executable(${TARGET_NAME}
        ${SOURCE_FILE}
        ${LIBROVA_QT_APP_DIR}/App/QtLaunchOptions.cpp
        ${LIBROVA_QT_APP_DIR}/App/QtRuntimePaths.cpp
        ${LIBROVA_QT_APP_DIR}/App/QtLogging.cpp
        ${LIBROVA_QT_APP_DIR}/App/QtRuntimeWorkspaceMaintenance.cpp
        ${LIBROVA_QT_APP_DIR}/App/QtApplicationBackend.cpp
        ${LIBROVA_QT_APP_DIR}/Controllers/PreferencesStore.cpp
        ${LIBROVA_QT_APP_DIR}/Controllers/ShellStateStore.cpp
        ${LIBROVA_QT_APP_DIR}/Controllers/ShellController.cpp
    )

    target_include_directories(${TARGET_NAME} PRIVATE
        ${LIBROVA_QT_APP_DIR}
    )

    target_link_libraries(${TARGET_NAME}
        PRIVATE
            Qt6::Core
            Qt6::Test
            LibrovaApp
            LibrovaFoundation
    )

    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)
    target_compile_definitions(${TARGET_NAME} PRIVATE
        LIBROVA_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )

    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /utf-8 /FS)
    endif()

    _librova_qt_register_test(${TARGET_NAME})
endfunction()

add_qt_test_with_app_sources(TestQtLaunchOptions TestQtLaunchOptions.cpp)
add_qt_test_with_app_sources(TestPreferencesStore TestPreferencesStore.cpp)
add_qt_test_with_app_sources(TestShellStateStore TestShellStateStore.cpp)
add_qt_test_with_app_sources(TestQtRuntimePaths TestQtRuntimePaths.cpp)
add_qt_test_with_app_sources(TestQtLogging TestQtLogging.cpp)
add_qt_test_with_app_sources(TestShellController TestShellController.cpp)
add_qt_test(TestQmlUiParity TestQmlUiParity.cpp)

# Phase E: model + adapter tests include production model/adapter sources directly.
function(add_qt_model_adapter_test TARGET_NAME SOURCE_FILE)
    qt_add_executable(${TARGET_NAME}
        ${SOURCE_FILE}
        ${LIBROVA_QT_APP_DIR}/Models/BookDetailsModel.cpp
        ${LIBROVA_QT_APP_DIR}/Models/BookListModel.cpp
        ${LIBROVA_QT_APP_DIR}/Models/CollectionListModel.cpp
        ${LIBROVA_QT_APP_DIR}/Models/FacetListModel.cpp
        ${LIBROVA_QT_APP_DIR}/Models/ImportJobListModel.cpp
        ${LIBROVA_QT_APP_DIR}/Adapters/QtCatalogAdapter.cpp
        ${LIBROVA_QT_APP_DIR}/Adapters/QtImportAdapter.cpp
        ${LIBROVA_QT_APP_DIR}/Adapters/QtExportAdapter.cpp
        ${LIBROVA_QT_APP_DIR}/Adapters/QtTrashAdapter.cpp
    )

    target_include_directories(${TARGET_NAME} PRIVATE
        ${LIBROVA_QT_APP_DIR}
    )

    target_link_libraries(${TARGET_NAME}
        PRIVATE
            Qt6::Core
            Qt6::Test
            LibrovaApp
            LibrovaFoundation
    )

    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)
    target_compile_definitions(${TARGET_NAME} PRIVATE
        LIBROVA_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )

    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /utf-8 /FS)
    endif()

    _librova_qt_register_test(${TARGET_NAME})
endfunction()

add_qt_model_adapter_test(TestBookListModel       TestBookListModel.cpp)
add_qt_model_adapter_test(TestCollectionListModel TestCollectionListModel.cpp)
add_qt_model_adapter_test(TestImportJobListModel  TestImportJobListModel.cpp)
add_qt_model_adapter_test(TestQtCatalogAdapter    TestQtCatalogAdapter.cpp)
add_qt_model_adapter_test(TestQtImportAdapter     TestQtImportAdapter.cpp)
add_qt_model_adapter_test(TestQtExportAdapter     TestQtExportAdapter.cpp)
add_qt_model_adapter_test(TestQtTrashAdapter      TestQtTrashAdapter.cpp)

# Controller-only tests: no App/ or Model/Adapter sources, no LibrovaApp needed.
function(add_qt_controller_test TARGET_NAME SOURCE_FILE)
    qt_add_executable(${TARGET_NAME}
        ${SOURCE_FILE}
        ${LIBROVA_QT_APP_DIR}/Controllers/FirstRunController.cpp
    )

    target_include_directories(${TARGET_NAME} PRIVATE
        ${LIBROVA_QT_APP_DIR}
    )

    target_link_libraries(${TARGET_NAME}
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)
    target_compile_definitions(${TARGET_NAME} PRIVATE
        LIBROVA_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )

    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /utf-8 /FS)
    endif()

    _librova_qt_register_test(${TARGET_NAME})
endfunction()

# Strong integration tests: combines App/ + Model/Adapter sources for real end-to-end flows.
# Uses QTEST_GUILESS_MAIN (QCoreApplication + event loop) to process cross-thread signals.
function(add_qt_strong_integration_test TARGET_NAME SOURCE_FILE)
    qt_add_executable(${TARGET_NAME}
        ${SOURCE_FILE}
        ${LIBROVA_QT_APP_DIR}/App/QtLaunchOptions.cpp
        ${LIBROVA_QT_APP_DIR}/App/QtRuntimePaths.cpp
        ${LIBROVA_QT_APP_DIR}/App/QtLogging.cpp
        ${LIBROVA_QT_APP_DIR}/App/QtRuntimeWorkspaceMaintenance.cpp
        ${LIBROVA_QT_APP_DIR}/App/QtApplicationBackend.cpp
        ${LIBROVA_QT_APP_DIR}/Models/BookDetailsModel.cpp
        ${LIBROVA_QT_APP_DIR}/Models/BookListModel.cpp
        ${LIBROVA_QT_APP_DIR}/Models/CollectionListModel.cpp
        ${LIBROVA_QT_APP_DIR}/Models/FacetListModel.cpp
        ${LIBROVA_QT_APP_DIR}/Models/ImportJobListModel.cpp
        ${LIBROVA_QT_APP_DIR}/Adapters/QtCatalogAdapter.cpp
        ${LIBROVA_QT_APP_DIR}/Adapters/QtImportAdapter.cpp
        ${LIBROVA_QT_APP_DIR}/Adapters/QtExportAdapter.cpp
        ${LIBROVA_QT_APP_DIR}/Adapters/QtTrashAdapter.cpp
    )

    target_include_directories(${TARGET_NAME} PRIVATE
        ${LIBROVA_QT_APP_DIR}
    )

    target_link_libraries(${TARGET_NAME}
        PRIVATE
            Qt6::Core
            Qt6::Test
            LibrovaApp
            LibrovaFoundation
    )

    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)
    target_compile_definitions(${TARGET_NAME} PRIVATE
        LIBROVA_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )

    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /utf-8 /FS)
    endif()

    _librova_qt_register_test(${TARGET_NAME})
endfunction()

add_qt_controller_test(TestFirstRunController TestFirstRunController.cpp)
add_qt_test_with_app_sources(TestQtRuntimeWorkspaceMaintenance TestQtRuntimeWorkspaceMaintenance.cpp)
add_qt_strong_integration_test(TestQtStrongIntegration TestQtStrongIntegration.cpp)
