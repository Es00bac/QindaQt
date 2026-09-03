# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(
    GLOB_RECURSE discovery_sources
    LIST_DIRECTORIES false
    "${SOURCE_ROOT}/src/services/display_color_discovery/*.h"
    "${SOURCE_ROOT}/src/services/display_color_discovery/*.cpp"
)
list(APPEND discovery_sources "${SOURCE_ROOT}/src/services/display_color_discovery/CMakeLists.txt")

# AGENT-GUARD: A matching file count proves the policy is not vacuously
# passing after a directory rename; the expected minimum grows only with the
# one public header, three sources with their private headers, and the
# registry file.
list(LENGTH discovery_sources discovery_source_count)
if(discovery_source_count LESS 6)
    message(FATAL_ERROR "Display color discovery boundary check found only ${discovery_source_count} files; expected at least 6")
endif()

foreach(source IN LISTS discovery_sources)
    file(READ "${source}" content)
    if(content MATCHES "<QtDBus/" OR content MATCHES "<QtQml/"
       OR content MATCHES "<QtQuick/" OR content MATCHES "<QtGui/"
       OR content MATCHES "<QtNetwork/"
       OR content MATCHES "kwin" OR content MATCHES "KWin"
       OR content MATCHES "wayland" OR content MATCHES "Wayland"
       OR content MATCHES "colord" OR content MATCHES "Colord"
       OR content MATCHES "qindaqt/services/display_(protocol|identity|topology|transaction|service|writer|journal|client|runtime)/"
       OR content MATCHES "QindaQt::Display(Protocol|Identity|Topology|Transaction|Service|Writer|Journal|Client|Runtime)"
       OR content MATCHES "qindaqt/services/display_color_assignment/"
       OR content MATCHES "QindaQt::Services::SettingsClient"
       OR content MATCHES "<QtCore/Q(FileDialog|Settings|Process)"
       OR content MATCHES "QProcess")
        message(FATAL_ERROR "Forbidden display-color-discovery dependency in ${source}")
    endif()
endforeach()

message(STATUS "Display color discovery boundary is pure presentation/transport/hardware independent")
