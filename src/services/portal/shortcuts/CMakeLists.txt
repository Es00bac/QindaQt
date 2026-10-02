# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_library(qindaqt_portal_shortcuts_policy STATIC shortcut_wire.cpp shortcut_wire.h)
add_library(QindaQt::PortalShortcutsPolicy ALIAS qindaqt_portal_shortcuts_policy)
target_include_directories(qindaqt_portal_shortcuts_policy PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_features(qindaqt_portal_shortcuts_policy PUBLIC cxx_std_20)
target_link_libraries(qindaqt_portal_shortcuts_policy PUBLIC Qt6::Core Qt6::Gui Qt6::DBus)
qindaqt_enable_warnings(qindaqt_portal_shortcuts_policy)
qt_add_library(qindaqt_portal_shortcuts STATIC
    global_shortcuts_adaptor.cpp global_shortcuts_adaptor.h shortcut_sessions.cpp shortcut_sessions_p.h process_shortcuts.cpp shortcut_ui.h)
add_library(QindaQt::PortalShortcuts ALIAS qindaqt_portal_shortcuts)
target_compile_features(qindaqt_portal_shortcuts PUBLIC cxx_std_20)
target_compile_definitions(qindaqt_portal_shortcuts PRIVATE QT_NO_KEYWORDS)
target_link_libraries(qindaqt_portal_shortcuts PUBLIC QindaQt::PortalShortcutsPolicy QindaQt::PortalSession QindaQt::PortalRequests QindaQt::PortalAccessPolicy QindaQt::ShortcutsClient)
qindaqt_enable_warnings(qindaqt_portal_shortcuts)
qt_add_executable(qindaqt-portal-shortcuts helper_main.cpp)
target_link_libraries(qindaqt-portal-shortcuts PRIVATE QindaQt::PortalShortcutsPolicy QindaQt::ShortcutsClient QindaQt::ForeignParent QindaQt::PortalRequests Qt6::Widgets)
qindaqt_enable_warnings(qindaqt-portal-shortcuts)
install(TARGETS qindaqt-portal-shortcuts RUNTIME DESTINATION "${KDE_INSTALL_LIBEXECDIR}" COMPONENT QindaQtPortalP0)

install(TARGETS qindaqt_portal_shortcuts_policy qindaqt_portal_shortcuts EXPORT QindaQtTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT QindaQtPortalP0)
