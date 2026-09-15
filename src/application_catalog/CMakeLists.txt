# SPDX-License-Identifier: LGPL-3.0-or-later

# Shared, apps-side installed-application catalog. Qt Core only; parsing and
# launch planning reuse the launcher's pure L0 (see ADR-0164). The shell
# launcher must never link this module: the dependency points apps <- shell
# launcher L0 only.
qt_add_library(qindaqt_application_catalog STATIC)
add_library(QindaQt::ApplicationCatalog ALIAS qindaqt_application_catalog)

target_sources(
    qindaqt_application_catalog
    PRIVATE
        src/application_directory_scan.cpp
        src/category_tree.cpp
        src/launch_support.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/application_catalog/application_directory_scan.h
            include/qindaqt/application_catalog/category_tree.h
            include/qindaqt/application_catalog/launch_support.h
)

target_link_libraries(qindaqt_application_catalog PUBLIC QindaQt::ShellLauncher)
target_compile_features(qindaqt_application_catalog PUBLIC cxx_std_20)
set_target_properties(qindaqt_application_catalog PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_application_catalog)

install(
    TARGETS qindaqt_application_catalog
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
