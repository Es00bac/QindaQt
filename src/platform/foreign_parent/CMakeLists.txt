# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui GuiPrivate WaylandClient WaylandScannerTools)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAL_WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
find_file(PORTAL_XDG_FOREIGN_XML xdg-foreign-unstable-v2.xml
    PATHS /usr/share/wayland-protocols/unstable/xdg-foreign REQUIRED)
enable_language(C)
qt_add_library(qindaqt_foreign_parent STATIC)
add_library(QindaQt::ForeignParent ALIAS qindaqt_foreign_parent)
target_sources(qindaqt_foreign_parent PRIVATE src/foreign_parent.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
    include/qindaqt/platform/foreign_parent/foreign_parent.h)
qt6_generate_wayland_protocol_client_sources(qindaqt_foreign_parent FILES "${PORTAL_XDG_FOREIGN_XML}")
# Scanner output is generated upstream protocol code, never hand-written policy.
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/qwayland-xdg-foreign-unstable-v2.cpp" PROPERTIES COMPILE_OPTIONS "-w")
target_include_directories(qindaqt_foreign_parent SYSTEM PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(qindaqt_foreign_parent PUBLIC Qt6::Core Qt6::Gui
    PRIVATE Qt6::GuiPrivate Qt6::WaylandClient PkgConfig::PORTAL_WAYLAND_CLIENT)
target_compile_features(qindaqt_foreign_parent PUBLIC cxx_std_20)
qindaqt_enable_warnings(qindaqt_foreign_parent)
install(TARGETS qindaqt_foreign_parent EXPORT QindaQtTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT QindaQtPortalP0
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" COMPONENT QindaQtPortalP0)
