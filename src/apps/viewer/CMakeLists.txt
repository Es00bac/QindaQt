# SPDX-License-Identifier: GPL-3.0-or-later

find_package(QindaTK CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(POPPLER_QT6 REQUIRED IMPORTED_TARGET poppler-qt6)

qt_add_library(qindaqt_viewer_support STATIC
    document_renderer.cpp document_renderer.h
    viewer_controller.cpp viewer_controller.h)
target_include_directories(qindaqt_viewer_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qindaqt_viewer_support PUBLIC Qt6::Core Qt6::Gui
    PRIVATE PkgConfig::POPPLER_QT6)
target_compile_features(qindaqt_viewer_support PUBLIC cxx_std_20)
qindaqt_enable_warnings(qindaqt_viewer_support)

qt_add_executable(qindaqt-viewer main.cpp frame_provider.h viewer_actions.cpp viewer_actions.h)
qt_add_qml_module(qindaqt-viewer
    URI QindaQt.Viewer VERSION 1.0 RESOURCE_PREFIX /qt/qml
    QML_FILES qml/Main.qml qml/ViewerMenuBar.qml)
target_link_libraries(qindaqt-viewer PRIVATE qindaqt_viewer_support
    QindaTK::qindatk QindaQt::AppShell QindaQt::AppShellMenuExport
    Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::DBus)
qindaqt_enable_warnings(qindaqt-viewer)

# AGENT-CONTRACT: AppShell's backing library is installed in the QML tree,
# outside the system library path. Preserve relocation into a staged prefix.
set(viewer_bindir "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}")
set(viewer_qmldir "${QT6_INSTALL_QML}")
cmake_path(ABSOLUTE_PATH viewer_qmldir BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}" NORMALIZE)
cmake_path(RELATIVE_PATH viewer_qmldir BASE_DIRECTORY "${viewer_bindir}"
    OUTPUT_VARIABLE viewer_qml_relative)
set_target_properties(qindaqt-viewer PROPERTIES
    INSTALL_RPATH "$ORIGIN/${viewer_qml_relative}/QindaQt/AppShell;$ORIGIN/${viewer_qml_relative}/QindaQt/Controls")
install(TARGETS qindaqt-viewer RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Viewer)
install(FILES org.qindaqt.Viewer.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications" COMPONENT Viewer)
install(FILES qindaqt-viewer.svg
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps" COMPONENT Viewer)
