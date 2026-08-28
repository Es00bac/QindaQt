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
    if(content MATCHES "QDBus|LayerShell|KWin::|QProcess|QSettings|QFileDialog|xdg-desktop-portal")
        message(FATAL_ERROR "${path}: AppShell crossed into a platform/service boundary")
    endif()
endforeach()

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
