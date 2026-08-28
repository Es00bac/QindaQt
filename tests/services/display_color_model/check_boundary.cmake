# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(
    GLOB_RECURSE color_sources
    LIST_DIRECTORIES false
    "${SOURCE_ROOT}/src/services/display_color_model/*.h"
    "${SOURCE_ROOT}/src/services/display_color_model/*.cpp"
)
list(APPEND color_sources "${SOURCE_ROOT}/src/services/display_color_model/CMakeLists.txt")

# AGENT-GUARD: A matching file count proves the policy is not vacuously
# passing after a directory rename; the expected minimum grows only with the
# four public headers, two sources, and the registry file.
list(LENGTH color_sources color_source_count)
if(color_source_count LESS 7)
    message(FATAL_ERROR "Display color model boundary check found only ${color_source_count} files; expected at least 7")
endif()

foreach(source IN LISTS color_sources)
    file(READ "${source}" content)
    if(content MATCHES "<QtDBus/" OR content MATCHES "<QtQml/"
       OR content MATCHES "<QtQuick/"
       OR content MATCHES "kwin" OR content MATCHES "KWin"
       OR content MATCHES "wayland" OR content MATCHES "Wayland"
       OR content MATCHES "qindaqt/services/display_(protocol|identity|topology|transaction|service)/"
       OR content MATCHES "QindaQt::Display(Protocol|Identity|Topology|Transaction|Service)"
       OR content MATCHES "<QtCore/Q(File|Settings|Timer|ElapsedTimer|Object|Process|Thread)")
        message(FATAL_ERROR "Forbidden display-color-model dependency in ${source}")
    endif()
endforeach()

message(STATUS "Display color model boundary is pure and presentation/transport/hardware independent")
