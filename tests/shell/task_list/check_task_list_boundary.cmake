# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

# AGENT-CONTRACT: The task-list source is a pure injected-facts module. It may
# consume Qt Core and its own public headers only; any platform mutation,
# presentation, transport, or private shell/compositor dependency is a
# boundary violation even when it compiles. Test suites are excluded because
# Qt Test itself legitimately requires QObject.
file(
    GLOB_RECURSE task_list_sources
    LIST_DIRECTORIES false
    "${SOURCE_ROOT}/src/shell/task_list/*.h"
    "${SOURCE_ROOT}/src/shell/task_list/*.cpp"
)
list(APPEND task_list_sources
    "${SOURCE_ROOT}/src/shell/task_list/CMakeLists.txt"
)

foreach(source IN LISTS task_list_sources)
    file(READ "${source}" content)
    if(content MATCHES "KWin" OR content MATCHES "kwin"
       OR content MATCHES "<QtGui/" OR content MATCHES "<QtWidgets/"
       OR content MATCHES "<QtQuick/" OR content MATCHES "<QtQml/"
       OR content MATCHES "<QtDBus/" OR content MATCHES "<QtNetwork/"
       OR content MATCHES "QDBus" OR content MATCHES "QProcess"
       OR content MATCHES "QTimer" OR content MATCHES "QObject"
       OR content MATCHES "QApplication" OR content MATCHES "QFile"
       OR content MATCHES "LayerShellQt" OR content MATCHES "layer-shell"
       OR content MATCHES "qindaqt/core/" OR content MATCHES "qindaqt/hybrid"
       OR content MATCHES "qindaqt/profiles/"
       OR content MATCHES "qindaqt/services/"
       OR content MATCHES "qindaqt/shell/app"
       OR content MATCHES "qindaqt/shell/common"
       OR content MATCHES "qindaqt/shell/runtime"
       OR content MATCHES "qindaqt/shell/launcher")
        message(FATAL_ERROR "Forbidden task-list dependency in ${source}")
    endif()
endforeach()

message(STATUS
    "Task list boundary is injected-facts, platform, and presentation independent")
