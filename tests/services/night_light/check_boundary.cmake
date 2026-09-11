# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(
    GLOB_RECURSE night_light_sources
    LIST_DIRECTORIES false
    "${SOURCE_ROOT}/src/services/night_light/*.h"
    "${SOURCE_ROOT}/src/services/night_light/*.cpp"
)
list(APPEND night_light_sources "${SOURCE_ROOT}/src/services/night_light/CMakeLists.txt")

# AGENT-GUARD: A matching file count proves the policy is not vacuously
# passing after a directory rename; the expected minimum grows only with the
# three public headers, one extra monitor header, four sources, and the
# registry file.
list(LENGTH night_light_sources night_light_source_count)
if(night_light_source_count LESS 9)
    message(FATAL_ERROR "Night light boundary check found only ${night_light_source_count} files; expected at least 9")
endif()

foreach(source IN LISTS night_light_sources)
    file(READ "${source}" content)
    if(content MATCHES "<QtQml/" OR content MATCHES "<QtQuick/"
       OR content MATCHES "<QtGui/" OR content MATCHES "<QtNetwork/"
       # The module may name the authorities in comments, but it must never
       # include, link, or shell out to them: KWin and knighttimed stay
       # foreign, and config paths arrive injected. KConfig value IO is the
       # one allowed KDE dependency; the watcher machinery stays out because
       # change truth comes from this module's own file watching.
       OR content MATCHES "#include <[Kk]win"
       OR content MATCHES "#include <KNightTime"
       OR content MATCHES "libKNightTime"
       OR content MATCHES "<KF6/" OR content MATCHES "KConfigWatcher"
       # Paths and environment are injected by the composition root; the
       # module never resolves its own locations or spawns processes.
       OR content MATCHES "QStandardPaths" OR content MATCHES "qgetenv"
       OR content MATCHES "getenv" OR content MATCHES "QProcess"
       # QindaQt service siblings stay out: no Display1/Settings/KWin-side
       # module may become a dependency of the bounded night light values.
       OR content MATCHES "qindaqt/services/(display_|settings_|power_|portal)"
       OR content MATCHES "QindaQt::(Display|Settings|Power)"
       OR content MATCHES "org.freedesktop.impl.portal")
        message(FATAL_ERROR "Forbidden night-light dependency in ${source}")
    endif()
endforeach()

message(STATUS "Night light boundary is pure values plus injected config/state transports")
