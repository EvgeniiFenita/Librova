cmake_minimum_required(VERSION 3.26)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 QuickDialogs2 QuickDialogs2QuickImpl)

qt_standard_project_setup(REQUIRES 6.5)

qt_policy(SET QTP0004 OLD)

# Bake version from VERSION.txt at compile time.
file(READ "${CMAKE_SOURCE_DIR}/VERSION.txt" LIBROVA_VERSION_RAW)
string(STRIP "${LIBROVA_VERSION_RAW}" LIBROVA_VERSION_STR)

# Singleton QML files must be marked before qt_add_qml_module.
set_source_files_properties(
    qml/theme/LibrovaTheme.qml
    qml/theme/LibrovaTypography.qml
    qml/theme/LibrovaIcons.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_executable(LibrovaQtApp
    main.cpp
    App/QtApplicationBackend.hpp
    App/QtApplicationBackend.cpp
    App/QtLaunchOptions.hpp
    App/QtLaunchOptions.cpp
    App/QtLogging.hpp
    App/QtLogging.cpp
    App/QtRuntimePaths.hpp
    App/QtRuntimePaths.cpp
    App/QtRuntimeWorkspaceMaintenance.hpp
    App/QtRuntimeWorkspaceMaintenance.cpp
    App/QtWindowsPlatform.hpp
    App/QtWindowsPlatform.cpp
    Controllers/PreferencesStore.hpp
    Controllers/PreferencesStore.cpp
    Controllers/ShellController.hpp
    Controllers/ShellController.cpp
    Controllers/ShellStateStore.hpp
    Controllers/ShellStateStore.cpp
    # Phase I: Controllers
    Controllers/ConverterValidationController.hpp
    Controllers/ConverterValidationController.cpp
    Controllers/FirstRunController.hpp
    Controllers/FirstRunController.cpp
    # Phase E: Models
    Models/BookDetailsModel.hpp
    Models/BookDetailsModel.cpp
    Models/BookListModel.hpp
    Models/BookListModel.cpp
    Models/CollectionListModel.hpp
    Models/CollectionListModel.cpp
    Models/FacetListModel.hpp
    Models/FacetListModel.cpp
    Models/ImportJobListModel.hpp
    Models/ImportJobListModel.cpp
    # Phase E: Adapters
    Adapters/QtStringConversions.hpp
    Adapters/QtCatalogAdapter.hpp
    Adapters/QtCatalogAdapter.cpp
    Adapters/QtImportAdapter.hpp
    Adapters/QtImportAdapter.cpp
    Adapters/QtExportAdapter.hpp
    Adapters/QtExportAdapter.cpp
    Adapters/QtTrashAdapter.hpp
    Adapters/QtTrashAdapter.cpp
)

qt_add_qml_module(LibrovaQtApp
    URI LibrovaQt
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/theme/LibrovaTheme.qml
        qml/theme/LibrovaTypography.qml
        qml/theme/LibrovaIcons.qml
        qml/components/LButton.qml
        qml/components/LNavItem.qml
        qml/components/LTextInput.qml
        qml/components/LGenreChip.qml
        qml/components/LIcon.qml
        qml/components/EmptyBookIllustration.qml
        qml/components/NoResultsIllustration.qml
        qml/components/CollectionCreateDialog.qml
        qml/components/LCheckBox.qml
        # Phase F: Shell UI
        qml/shell/AppShell.qml
        qml/shell/SidebarNav.qml
        qml/shell/SectionLoader.qml
        qml/shell/CurrentLibraryPanel.qml
        qml/shell/OpeningView.qml
        qml/shell/FirstRunView.qml
        qml/shell/StartupRecoveryView.qml
        qml/shell/FatalErrorView.qml
        # Phase F: Sections
        qml/sections/LibrarySection.qml
        qml/sections/ImportSection.qml
        qml/sections/SettingsSection.qml
        # Phase G: Library View
        qml/library/BookCard.qml
        qml/library/BookGrid.qml
        qml/library/LibraryToolbar.qml
        qml/library/BookDetailsPanel.qml
        qml/library/BookContextMenu.qml
        # Phase H: Import View
        qml/import/ImportView.qml
        qml/import/DropZoneOverlay.qml
        # Phase I: Settings View
        qml/settings/SettingsView.qml
        # Phase J: new component
        qml/components/LToast.qml
        qml/components/LScrollBar.qml
)

qt_add_resources(LibrovaQtApp "appresources"
    PREFIX "/"
    FILES
        assets/librova.ico
        assets/librova_hero.png
        assets/empty_library.png
        assets/no_results.png
        assets/no_collections.png
        assets/brand_badge.png
        assets/import_drop.png
)

if(WIN32)
    target_sources(LibrovaQtApp PRIVATE app.rc)
endif()

target_include_directories(LibrovaQtApp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(LibrovaQtApp PRIVATE
    LIBROVA_VERSION="${LIBROVA_VERSION_STR}"
)

target_link_libraries(LibrovaQtApp
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickDialogs2
        Qt6::QuickDialogs2QuickImpl
        LibrovaApp
        LibrovaFoundation
        dwmapi
        Ole32
)

set_target_properties(LibrovaQtApp PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_compile_features(LibrovaQtApp PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(LibrovaQtApp PRIVATE /utf-8)
endif()

