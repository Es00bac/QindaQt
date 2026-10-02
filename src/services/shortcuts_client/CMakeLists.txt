# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_library(qindaqt_shortcuts_client STATIC)
add_library(QindaQt::ShortcutsClient ALIAS qindaqt_shortcuts_client)
target_sources(qindaqt_shortcuts_client PRIVATE transport.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES include/qindaqt/services/shortcuts_client/transport.h)
target_include_directories(qindaqt_shortcuts_client PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(qindaqt_shortcuts_client PUBLIC cxx_std_20)
target_compile_definitions(qindaqt_shortcuts_client PRIVATE QT_NO_KEYWORDS)
target_link_libraries(qindaqt_shortcuts_client PUBLIC Qt6::Core Qt6::Gui Qt6::DBus)
qindaqt_enable_warnings(qindaqt_shortcuts_client)

install(TARGETS qindaqt_shortcuts_client EXPORT QindaQtTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT QindaQtPortalP0
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" COMPONENT QindaQtPortalP0)
