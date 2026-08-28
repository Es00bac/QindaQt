# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_global_menu_applet STATIC)
add_library(QindaQt::GlobalMenuApplet ALIAS qindaqt_global_menu_applet)

target_sources(
    qindaqt_global_menu_applet
    PRIVATE
        src/globalmenuappletaccess.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/global_menu/applet/globalmenuappletaccess.h
)
target_link_libraries(qindaqt_global_menu_applet PUBLIC QindaQt::GlobalMenuProtocol Qt6::Core)
target_compile_features(qindaqt_global_menu_applet PUBLIC cxx_std_20)
set_target_properties(qindaqt_global_menu_applet PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_global_menu_applet)

install(
    TARGETS qindaqt_global_menu_applet
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
