# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

# AGENT-CONTRACT: The task-list core is a pure injected-facts module
# (ADR-0044). It may consume Qt Core and its own public headers only; any
# platform mutation, presentation, transport, or private shell/compositor
# dependency is a boundary violation even when it compiles. Test suites are
# excluded because Qt Test itself legitimately requires QObject.
file(
    GLOB task_list_core_sources
    LIST_DIRECTORIES false
    "${SOURCE_ROOT}/src/shell/task_list/include/qindaqt/shell/task_list/*.h"
    "${SOURCE_ROOT}/src/shell/task_list/src/*.h"
    "${SOURCE_ROOT}/src/shell/task_list/src/*.cpp"
)
list(APPEND task_list_core_sources
    "${SOURCE_ROOT}/src/shell/task_list/CMakeLists.txt"
)

foreach(source IN LISTS task_list_core_sources)
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

# AGENT-CONTRACT: The producer and operations submodules are the task list's
# only transport layer: they consume the public Compositor1 D-Bus authority
# through injected connections and must never touch KWin, the shell runtime,
# QML, the filesystem, or process control. QObject/QTimer/QtDBus are the
# transport's own tools and are permitted here (and nowhere else in the
# module).
file(
    GLOB_RECURSE task_list_transport_sources
    LIST_DIRECTORIES false
    "${SOURCE_ROOT}/src/shell/task_list/producer/*.h"
    "${SOURCE_ROOT}/src/shell/task_list/producer/*.cpp"
    "${SOURCE_ROOT}/src/shell/task_list/operations/*.h"
    "${SOURCE_ROOT}/src/shell/task_list/operations/*.cpp"
)
list(APPEND task_list_transport_sources
    "${SOURCE_ROOT}/src/shell/task_list/producer/CMakeLists.txt"
    "${SOURCE_ROOT}/src/shell/task_list/operations/CMakeLists.txt"
)

foreach(source IN LISTS task_list_transport_sources)
    file(READ "${source}" content)
    if(content MATCHES "KWin" OR content MATCHES "kwin"
       OR content MATCHES "<QtGui/" OR content MATCHES "<QtWidgets/"
       OR content MATCHES "<QtQuick/" OR content MATCHES "<QtQml/"
       OR content MATCHES "<QtNetwork/"
       OR content MATCHES "QProcess" OR content MATCHES "QApplication"
       OR content MATCHES "QFile" OR content MATCHES "LayerShellQt"
       OR content MATCHES "layer-shell"
       OR content MATCHES "qindaqt/core/" OR content MATCHES "qindaqt/hybrid"
       OR content MATCHES "qindaqt/profiles/"
       OR content MATCHES "qindaqt/services/"
       OR content MATCHES "qindaqt/shell/app"
       OR content MATCHES "qindaqt/shell/common"
       OR content MATCHES "qindaqt/shell/runtime"
       OR content MATCHES "qindaqt/shell/launcher")
        message(FATAL_ERROR "Forbidden task-list transport dependency in ${source}")
    endif()
endforeach()

# The pure decoding policy must stay transport-free inside the producer.
foreach(source IN LISTS task_list_transport_sources)
    if(source MATCHES "task_list_wire\\." OR source MATCHES "task_list_operations\\.h")
        file(READ "${source}" content)
        if(content MATCHES "QDBus" OR content MATCHES "<QtDBus/"
           OR content MATCHES "QObject" OR content MATCHES "QTimer")
            message(FATAL_ERROR "Pure task-list component has a transport dependency in ${source}")
        endif()
    endif()
endforeach()

# AGENT-NOTE: Review finding P1-1 on rejected candidate 3a5ae17. The public
# Compositor1 contract forbids joining its panel-visibility snapshot to the
# independent Windows() inventory, even when their fences happen to match.
file(
    GLOB_RECURSE task_list_producer_sources
    LIST_DIRECTORIES false
    "${SOURCE_ROOT}/src/shell/task_list/producer/*.h"
    "${SOURCE_ROOT}/src/shell/task_list/producer/*.cpp"
)
foreach(source IN LISTS task_list_producer_sources)
    file(READ "${source}" content)
    string(CONCAT forbidden_method "ShellVisibility" "Snapshot")
    if(content MATCHES "${forbidden_method}")
        message(FATAL_ERROR
            "Task-list producer must not consume panel-visibility inventory: ${source}"
        )
    endif()
endforeach()

# AGENT-NOTE: Review finding P3-1 on rejected candidate 3a5ae17 predated the
# authenticated window-actions and active-identity boundaries. Keep the owning
# page pointed at the accepted composition path instead of requesting another
# Compositor1 extension.
file(READ "${SOURCE_ROOT}/docs/wiki/shell/task-list.md" task_list_wiki)
foreach(required_marker IN ITEMS
        "src/shell_window_actions_client"
        "ADR-0061"
        "ADR-0063")
    if(NOT task_list_wiki MATCHES "${required_marker}")
        message(FATAL_ERROR
            "Task-list wiki is missing current window-action marker: ${required_marker}"
        )
    endif()
endforeach()

message(STATUS
    "Task list boundary is injected-facts, platform, and presentation independent")
