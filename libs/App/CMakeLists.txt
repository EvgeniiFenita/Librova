add_library(LibrovaApp STATIC
    CLibraryApplication.cpp
    CLibraryApplication.hpp
    ILibraryApplication.hpp
    ImportJobManager.cpp
    ImportJobManager.hpp
    ImportJobRunner.cpp
    ImportJobRunner.hpp
    ImportJobService.cpp
    ImportJobService.hpp
    ImportRollbackService.cpp
    ImportRollbackService.hpp
    ImportWorkloadPlanner.cpp
    ImportWorkloadPlanner.hpp
    LibraryBootstrap.cpp
    LibraryBootstrap.hpp
    LibraryCatalogFacade.cpp
    LibraryCatalogFacade.hpp
    LibraryExportFacade.cpp
    LibraryExportFacade.hpp
    LibraryImportFacade.cpp
    LibraryImportFacade.hpp
    LibraryTrashFacade.cpp
    LibraryTrashFacade.hpp
    StructuredProgressMapper.cpp
    StructuredProgressMapper.hpp
)

target_include_directories(LibrovaApp
    PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(LibrovaApp
    PUBLIC
        LibrovaDomain
        LibrovaImport
        LibrovaConverter
    PRIVATE
        LibrovaDatabase
        LibrovaParsing
        LibrovaFoundation
        LibrovaStorage
)

target_compile_features(LibrovaApp PUBLIC cxx_std_20)
