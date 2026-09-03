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
