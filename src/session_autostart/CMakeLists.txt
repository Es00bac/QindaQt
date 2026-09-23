# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_library(qindaqt_session_autostart STATIC)
add_library(QindaQt::SessionAutostart ALIAS qindaqt_session_autostart)
target_sources(qindaqt_session_autostart
    PRIVATE src/autostart_catalog.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include
    FILES include/qindaqt/session_autostart/autostart_catalog.h)
target_link_libraries(qindaqt_session_autostart PUBLIC QindaQt::ShellLauncher Qt6::Core)
target_compile_features(qindaqt_session_autostart PUBLIC cxx_std_20)
set_target_properties(qindaqt_session_autostart PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_session_autostart)
install(TARGETS qindaqt_session_autostart EXPORT QindaQtTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
