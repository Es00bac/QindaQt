# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(
    GLOB_RECURSE assignment_sources
    LIST_DIRECTORIES false
    "${SOURCE_ROOT}/src/services/display_color_assignment/*.h"
    "${SOURCE_ROOT}/src/services/display_color_assignment/*.cpp"
)
list(APPEND assignment_sources "${SOURCE_ROOT}/src/services/display_color_assignment/CMakeLists.txt")

# AGENT-GUARD: A matching file count proves the policy is not vacuously
# passing after a directory rename; the expected minimum grows only with the
# two public headers, two sources, and the registry file.
list(LENGTH assignment_sources assignment_source_count)
if(assignment_source_count LESS 5)
    message(FATAL_ERROR "Display color assignment boundary check found only ${assignment_source_count} files; expected at least 5")
endif()

foreach(source IN LISTS assignment_sources)
    file(READ "${source}" content)
    if(content MATCHES "<QtDBus/" OR content MATCHES "<QtQml/"
       OR content MATCHES "<QtQuick/" OR content MATCHES "<QtGui/"
       OR content MATCHES "<QtNetwork/"
       OR content MATCHES "kwin" OR content MATCHES "KWin"
       OR content MATCHES "wayland" OR content MATCHES "Wayland"
       OR content MATCHES "colord" OR content MATCHES "Colord"
       OR content MATCHES "qindaqt/services/display_(protocol|identity|topology|transaction|service|writer|journal|client|runtime)/"
       OR content MATCHES "QindaQt::Display(Protocol|Identity|Topology|Transaction|Service|Writer|Journal|Client|Runtime)"
       OR content MATCHES "qindaqt/services/display_color_discovery/"
       OR content MATCHES "qindaqt/services/settings_service/"
       OR content MATCHES "<QtCore/Q(FileDialog|Settings|Process)"
       OR content MATCHES "QProcess")
        message(FATAL_ERROR "Forbidden display-color-assignment dependency in ${source}")
    endif()
endforeach()

message(STATUS "Display color assignment boundary is presentation/platform independent and transports only through the public settings client")
