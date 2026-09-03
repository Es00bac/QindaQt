# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

# AGENT-GUARD: The resident Power1 orchestration (everything outside adapters/)
# stays Wayland-free and host-free by contract: it must never mention upstream
# daemons, sysfs paths, processes, or threads. The production adapters under
# adapters/ are the only place allowed to name org.freedesktop.UPower,
# org.freedesktop.login1, power-profiles-daemon, or a sysfs root, and each
# adapter family is additionally fenced against the other families' transports
# (see ADR-0060). These poison negatives fail the change that violates them.
set(power_paths
    "${SOURCE_ROOT}/src/services/power_service"
    "${SOURCE_ROOT}/src/services/power_client")

# Forbidden everywhere in the power service and client, including adapters.
set(common_forbidden_patterns
    "wayland-client"
    "QtWayland"
    "<QtGui/"
    "<QtQml/"
    "<QtQuick/"
    "QProcess"
    "QThread"
    "brightness_model"
    "audio_"
    "display_"
    "session_supervisor"
    "sd-login"
    "systemd/sd"
    "libupower")

# Forbidden in orchestration (non-adapter) files: naming an upstream daemon,
# bus path, or sysfs root there reintroduces the PB-1 violation.
set(core_forbidden_patterns
    ${common_forbidden_patterns}
    "upower"
    "login1"
    "org.freedesktop.login1"
    "org.freedesktop.UPower"
    "net.hadess.PowerProfiles"
    "/sys/class")

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
        if(source MATCHES "CMakeLists\\.txt$")
            # Build manifests legitimately list adapter sources; they are not
            # orchestration code and are fenced with the common patterns only.
            set(forbidden_patterns "${common_forbidden_patterns}")
            set(scope "manifest")
        elseif(source MATCHES "/adapters/")
            set(forbidden_patterns "${common_forbidden_patterns}")
            # Each adapter family may use only its own transport.
            if(source MATCHES "/sysfs_backlight_source[^/]*$")
                list(APPEND forbidden_patterns
                    "upower" "login1" "org.freedesktop.login1"
                    "org.freedesktop.UPower" "net.hadess.PowerProfiles" "<QtDBus/")
            elseif(source MATCHES "/(upower_|power_profiles_)[^/]*$")
                list(APPEND forbidden_patterns
                    "login1" "org.freedesktop.login1" "/sys/class")
            elseif(source MATCHES "/logind_[^/]*$")
                list(APPEND forbidden_patterns
                    "upower" "org.freedesktop.UPower" "net.hadess.PowerProfiles"
                    "/sys/class")
            endif()
            set(scope "adapter")
        else()
            set(forbidden_patterns "${core_forbidden_patterns}")
            set(scope "core")
        endif()
        foreach(pattern IN LISTS forbidden_patterns)
            if(content MATCHES "${pattern}")
                message(FATAL_ERROR
                    "Forbidden Power ${scope} dependency '${pattern}' in ${source}")
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

message(STATUS "Power boundary holds: core host-free, adapters transport-fenced")
