# SPDX-License-Identifier: GPL-3.0-or-later

# The desktop controls aggregate EXISTING facades. Their sources may include
# the public facade headers, the workspace controller, Qt Core, and (for the
# QML module) Qt Quick. They must never open a bus, name KWin or LayerShellQt,
# include the shell runtime, or start a process outside the launcher's seam.
if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "Missing SOURCE_ROOT")
endif()

file(GLOB_RECURSE controller_sources
    "${SOURCE_ROOT}/src/shell/desktop_controls/src/*.cpp"
    "${SOURCE_ROOT}/src/shell/desktop_controls/include/*.h")
file(GLOB qml_sources "${SOURCE_ROOT}/src/shell/desktop_controls/qml/*.qml")

set(forbidden_cpp
    "<QDBus" "QtDBus" "QDBusConnection" "<KWin" "kwin/" "LayerShellQt"
    "shellruntimeapplication" "<QProcess>" "QProcess::" "startDetached"
    "qt_workspace_transport" "org.kde.KWin")
set(forbidden_qml
    "import QtQuick.Controls 2" "#[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]\""
    "popupType: T.Popup.Item" "Qt.createQmlObject")

foreach(source IN LISTS controller_sources)
    file(READ "${source}" contents)
    foreach(token IN LISTS forbidden_cpp)
        string(FIND "${contents}" "${token}" hit)
        if(NOT hit EQUAL -1)
            message(FATAL_ERROR "${source} must not reference '${token}'")
        endif()
    endforeach()
endforeach()

foreach(source IN LISTS qml_sources)
    file(READ "${source}" contents)
    foreach(token IN LISTS forbidden_qml)
        string(REGEX MATCH "${token}" hit "${contents}")
        if(hit)
            message(FATAL_ERROR "${source} must not contain '${token}' (tokens-only presentation, window popups)")
        endif()
    endforeach()
endforeach()

# Every outward-facing popup in the module is a separate popup window.
foreach(popup_source IN ITEMS ControlPopupFrame.qml QuickLaunchApplet.qml SystemMenuApplet.qml)
    file(READ "${SOURCE_ROOT}/src/shell/desktop_controls/qml/${popup_source}" contents)
    string(FIND "${contents}" "popupType: T.Popup.Window" hit)
    if(hit EQUAL -1)
        message(FATAL_ERROR "${popup_source} must host its popup in a separate window")
    endif()
endforeach()

# The only process start is the launcher's seam, fed absolute candidates.
file(READ "${SOURCE_ROOT}/src/shell/desktop_controls/src/file_manager_folder_opener.cpp" opener)
foreach(required IN ITEMS "m_spawner.spawn" "info.isAbsolute()" "info.isExecutable()")
    string(FIND "${opener}" "${required}" hit)
    if(hit EQUAL -1)
        message(FATAL_ERROR "file_manager_folder_opener.cpp lacks required contract: ${required}")
    endif()
endforeach()
message(STATUS "Desktop controls boundary contracts passed")
