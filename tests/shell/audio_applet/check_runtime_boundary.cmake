# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "Missing Audio applet runtime source root")
endif()

set(runtime_root "${SOURCE_ROOT}/src/shell/audio_applet")
file(GLOB applet_runtime_sources
    "${runtime_root}/audio_applet_controller.h"
    "${runtime_root}/audio_applet_controller.cpp"
    "${runtime_root}/qml/*.qml")
set(composition_sources "")
foreach(candidate IN ITEMS
        "${SOURCE_ROOT}/src/shell/runtime/audioappletcomposition.h"
        "${SOURCE_ROOT}/src/shell/runtime/audioappletcomposition.cpp")
    if(EXISTS "${candidate}")
        list(APPEND composition_sources "${candidate}")
    endif()
endforeach()
set(runtime_sources ${applet_runtime_sources} ${composition_sources})
list(LENGTH runtime_sources runtime_source_count)
if(runtime_source_count EQUAL 0)
    message(FATAL_ERROR "Audio applet runtime boundary found no sources")
endif()

# AGENT-NOTE: The bare protocol reason codes "pipewire-replaced" and
# "wireplumber-replaced" are public Audio1 contract vocabulary and appear in
# the controller's feedback mapping by design. The gate targets
# service-internal surfaces instead: implementation headers, the WirePlumber
# library, GLib, and process/filesystem escapes.
set(forbidden
    "audio_service"
    "AudioService"
    "services/audio_service"
    "libwireplumber"
    "wireplumber_audio_backend"
    "pipewire/pipewire.h"
    "glib.h"
    "wp_object"
    "/sys/"
    "QProcess"
    "QFile")
set(violations "")
foreach(path IN LISTS runtime_sources)
    file(READ "${path}" content)
    foreach(token IN LISTS forbidden)
        string(FIND "${content}" "${token}" hit)
        if(NOT hit EQUAL -1)
            list(APPEND violations "${path}: forbidden token '${token}'")
        endif()
    endforeach()
endforeach()

# The production composition may construct the public QtAudioTransport on
# the session bus. That narrow root does not authorize D-Bus in the
# controller or QML renderer.
foreach(path IN LISTS applet_runtime_sources)
    file(READ "${path}" content)
    foreach(token IN ITEMS "QtDBus" "QDBus")
        string(FIND "${content}" "${token}" hit)
        if(NOT hit EQUAL -1)
            list(APPEND violations "${path}: forbidden token '${token}'")
        endif()
    endforeach()
endforeach()

if(violations)
    foreach(violation IN LISTS violations)
        message(SEND_ERROR "${violation}")
    endforeach()
    message(FATAL_ERROR "Audio applet runtime boundary failed")
endif()

# Mutation-sensitive negative control: the same checker must reject a planted
# service-internal dependency. The fixture stays under the build tree.
if(DEFINED POISON_ROOT AND NOT RUNTIME_POLICY_SKIP_POISON)
    cmake_path(NORMAL_PATH POISON_ROOT OUTPUT_VARIABLE poison_root)
    file(REMOVE_RECURSE "${poison_root}")
    file(MAKE_DIRECTORY "${poison_root}/src/shell/audio_applet")
    file(WRITE "${poison_root}/src/shell/audio_applet/audio_applet_controller.cpp"
         "#include <qindaqt/services/audio_service/resident_audio_service.h>\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
                "-DSOURCE_ROOT=${poison_root}"
                -DRUNTIME_POLICY_SKIP_POISON=ON
                -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE poison_status
        OUTPUT_VARIABLE poison_output
        ERROR_VARIABLE poison_error)
    file(REMOVE_RECURSE "${poison_root}")
    if(poison_status EQUAL 0)
        message(FATAL_ERROR
            "Audio applet runtime boundary accepted service-internal poison:\n"
            "${poison_output}${poison_error}")
    endif()
endif()

message(STATUS
    "Audio applet runtime boundary passed (${runtime_source_count} files and poison rejection)")
