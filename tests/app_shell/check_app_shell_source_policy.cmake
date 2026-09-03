# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED APP_SHELL_SOURCE_DIR)
    message(FATAL_ERROR "APP_SHELL_SOURCE_DIR is required")
endif()
file(GLOB_RECURSE production_files
     "${APP_SHELL_SOURCE_DIR}/*.cpp"
     "${APP_SHELL_SOURCE_DIR}/*.h"
     "${APP_SHELL_SOURCE_DIR}/*.qml")
if(production_files STREQUAL "")
    message(FATAL_ERROR "AppShell source policy found no production files")
endif()

foreach(path IN LISTS production_files)
    file(READ "${path}" content)
    if(NOT path MATCHES "/menu_export/" AND
       content MATCHES "QDBus|LayerShell|KWin::|QProcess|QSettings|QFileDialog|xdg-desktop-portal")
        message(FATAL_ERROR "${path}: AppShell crossed into a platform/service boundary")
    endif()
endforeach()

function(menu_export_has_forbidden content result)
    if(content MATCHES "QDBusConnection::sessionBus\\(|qindaqt/shell/runtime|KWin::|QProcess|QSettings|QFileDialog|xdg-desktop-portal")
        set(${result} TRUE PARENT_SCOPE)
    else()
        set(${result} FALSE PARENT_SCOPE)
    endif()
endfunction()

file(GLOB_RECURSE menu_export_files
     "${APP_SHELL_SOURCE_DIR}/menu_export/*.cpp"
     "${APP_SHELL_SOURCE_DIR}/menu_export/*.h")
if(menu_export_files STREQUAL "")
    message(FATAL_ERROR "AppShell menu-export boundary found no production files")
endif()
foreach(path IN LISTS menu_export_files)
    file(READ "${path}" content)
    menu_export_has_forbidden("${content}" forbidden)
    if(forbidden)
        message(FATAL_ERROR "${path}: menu export bypassed its injected transport boundary")
    endif()
    if(NOT path MATCHES "/qt_window_menu_identity\\.cpp$" AND
       content MATCHES "qdesktopunixservices|qwaylandwindow")
        message(FATAL_ERROR "${path}: Qt private identity hook escaped its one adapter")
    endif()
endforeach()

# AGENT-NOTE: P1-02/P1-03 regression proof. Application exporters transport
# content; the shell selector remains the only lineage issuer, and the complete
# standard server remains owned by the accepted global-menu dbusmenu module.
set(menu_export_combined "")
foreach(path IN LISTS menu_export_files)
    file(READ "${path}" content)
    string(APPEND menu_export_combined "\n${content}")
endforeach()
if(menu_export_combined MATCHES "LocalExportLineage|QUuid::createUuid|ExportLineageSource|menu_exporter\\.h|GlobalMenuExporter")
    message(FATAL_ERROR "AppShell menu export minted or imported shell lineage authority")
endif()
if(menu_export_combined MATCHES "Q_CLASSINFO\\(.*com\\.canonical\\.dbusmenu")
    message(FATAL_ERROR "AppShell declared a second dbusmenu server")
endif()
if(NOT menu_export_combined MATCHES "global_menu/dbusmenu/dbusmenu_server\\.h")
    message(FATAL_ERROR "AppShell did not consume the accepted dbusmenu server boundary")
endif()

# Negative control: prove the matcher rejects service lookup rather than
# passing because the production tree happens not to contain the spelling.
menu_export_has_forbidden("QDBusConnection::sessionBus()" poison_detected)
if(NOT poison_detected)
    message(FATAL_ERROR "menu-export source-policy poison was not detected")
endif()
menu_export_has_forbidden("QDBusConnection injectedBus" clean_detected)
if(clean_detected)
    message(FATAL_ERROR "menu-export source-policy rejected its injected bus seam")
endif()

file(READ "${APP_SHELL_SOURCE_DIR}/qml/ApplicationShell.qml" qml)
foreach(required IN ITEMS "QindaQt.Tokens 1.0" "QindaQt.Controls 1.0"
                          "Accessible.name" "initialFocusItem" "requestQuit")
    if(NOT qml MATCHES "${required}")
        message(FATAL_ERROR "ApplicationShell.qml is missing ${required}")
    endif()
endforeach()
if(qml MATCHES "#[0-9A-Fa-f]{3,8}|sourceThemeId|qinda-(dark|light|dusk|macos)")
    message(FATAL_ERROR "ApplicationShell.qml contains palette or theme identity policy")
endif()
