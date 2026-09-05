# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED QINDAQT_SOURCE_DIR)
    message(FATAL_ERROR "QINDAQT_SOURCE_DIR is required")
endif()

file(READ "${QINDAQT_SOURCE_DIR}/src/shell/audio_applet/qml/AudioApplet.qml"
     audio_qml)

foreach(required IN ITEMS
        "rows[i].isOutput === true"
        "rows[i].isDefault === true"
        "outputRow.muted"
        "outputRow.volume"
        "audio-volume-muted"
        "audio-volume-low"
        "audio-volume-medium"
        "audio-volume-high")
    string(FIND "${audio_qml}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "Audio summary icon contract is missing '${required}'")
    endif()
endforeach()

if(audio_qml MATCHES "rows\\[i\\]\\.direction|outputRow\\.normalizedVolume")
    message(FATAL_ERROR
        "Audio summary icon uses a property absent from the DeviceRow gadget")
endif()
