# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

# AGENT-GUARD: PB-1 is Wayland-free and host-free by contract. These poison
# negatives keep a future adapter edit from silently importing host UPower,
# logind, Wayland, sysfs, process, thread, or sibling-service implementation
# dependencies into the resident service or client. Real adapters arrive in
# later slices and must live in their own modules or justify an ADR.
set(power_paths
    "${SOURCE_ROOT}/src/services/power_service"
    "${SOURCE_ROOT}/src/services/power_client")
set(forbidden_patterns
    "upower"
    "libupower"
    "sd-login"
    "systemd/sd"
    "login1"
    "org.freedesktop.login1"
    "org.freedesktop.UPower"
    "net.hadess.PowerProfiles"
    "wayland-client"
    "QtWayland"
    "<QtGui/"
    "<QtQml/"
    "<QtQuick/"
    "QProcess"
    "QThread"
    "/sys/class"
    "brightness_model"
    "audio_"
    "display_"
    "session_supervisor")

foreach(root IN LISTS power_paths)
    file(
        GLOB_RECURSE power_sources
        LIST_DIRECTORIES false
        "${root}/*.h"
        "${root}/*.cpp"
    )
    list(APPEND power_sources "${root}/CMakeLists.txt")
    foreach(source IN LISTS power_sources)
        file(READ "${source}" content)
        foreach(pattern IN LISTS forbidden_patterns)
            if(content MATCHES "${pattern}")
                message(FATAL_ERROR
                    "Forbidden Power PB-1 dependency '${pattern}' in ${source}")
            endif()
        endforeach()
    endforeach()
endforeach()

# The client must not reach into service implementation details and the
# service must not depend on its own client.
file(
    GLOB_RECURSE client_sources
    LIST_DIRECTORIES false
    "${SOURCE_ROOT}/src/services/power_client/*.h"
    "${SOURCE_ROOT}/src/services/power_client/*.cpp"
)
foreach(source IN LISTS client_sources)
    file(READ "${source}" content)
    if(content MATCHES "services/power_service/")
        message(FATAL_ERROR "Power client depends on service implementation in ${source}")
    endif()
endforeach()

file(
    GLOB_RECURSE service_sources
    LIST_DIRECTORIES false
    "${SOURCE_ROOT}/src/services/power_service/*.h"
    "${SOURCE_ROOT}/src/services/power_service/*.cpp"
)
foreach(source IN LISTS service_sources)
    file(READ "${source}" content)
    if(content MATCHES "services/power_client/")
        message(FATAL_ERROR "Power service depends on its client in ${source}")
    endif()
endforeach()

message(STATUS "Power PB-1 boundary is Wayland-free, host-free, and implementation-separated")
