# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(route_root "${SOURCE_ROOT}/src/apps/settings/audio")
file(GLOB_RECURSE route_cpp LIST_DIRECTORIES false
     "${route_root}/*.h" "${route_root}/*.cpp")
foreach(source IN LISTS route_cpp)
    file(READ "${source}" content)
    if(content MATCHES "services/audio_service/"
       OR content MATCHES "<wp/|<pipewire/|wireplumber|WpCore|pw_core|QtDBus|QDBus")
        message(FATAL_ERROR
            "Audio Settings crossed the public AudioClient boundary in ${source}")
    endif()
    if(content MATCHES "Q_SLOT|Q_SLOTS")
        message(FATAL_ERROR
            "Audio Settings exposed a callable slot outside its closed intent surface in ${source}")
    endif()
    string(REGEX MATCHALL "Q_INVOKABLE[^;]*\\)" invokables "${content}")
    set(allowed_invokables
        "Q_INVOKABLE bool reload()"
        "Q_INVOKABLE bool setDefaultDevice(quint64 serial)"
        "Q_INVOKABLE bool setDeviceVolume(quint64 serial, double level)"
        "Q_INVOKABLE bool setDeviceMuted(quint64 serial, bool muted)"
        "Q_INVOKABLE bool setStreamVolume(quint64 serial, double level)"
        "Q_INVOKABLE bool setStreamMuted(quint64 serial, bool muted)"
        "Q_INVOKABLE bool setDeviceChannelVolume(quint64 serial, int channelIndex, double level)"
        "Q_INVOKABLE bool createVirtualDevice(QString kindToken, QString displayName, int channels)"
        "Q_INVOKABLE bool removeVirtualDevice(quint64 serial)"
    )
    foreach(invokable IN LISTS invokables)
        string(REGEX REPLACE "[ \t\r\n]+" " " normalized "${invokable}")
        string(STRIP "${normalized}" normalized)
        list(FIND allowed_invokables "${normalized}" allowed_index)
        if(allowed_index EQUAL -1)
            message(FATAL_ERROR
                "Audio Settings exposed an invokable outside its closed intent surface in ${source}: ${normalized}")
        endif()
    endforeach()
endforeach()

file(GLOB_RECURSE route_qml LIST_DIRECTORIES false "${route_root}/qml/*.qml")
foreach(source IN LISTS route_qml)
    file(READ "${source}" content)
    if(content MATCHES "TextField|TextInput|TextEdit|TextArea|moveStream|MoveStream")
        message(FATAL_ERROR
            "Audio Settings QML gained text entry or an out-of-slice stream-move surface in ${source}")
    endif()
endforeach()

set(route_cmake "${route_root}/CMakeLists.txt")
if(EXISTS "${route_cmake}")
    file(READ "${route_cmake}" cmake_content)
    if(cmake_content MATCHES "AudioService|WirePlumber|PipeWire|QtAudioTransport")
        message(FATAL_ERROR
            "Audio Settings domain target reversed the public client boundary")
    endif()
    foreach(required IN ITEMS
            "QindaQt::AudioClient"
            "qindaqt_settings_audio_qml"
            "AudioPage.qml"
            "COMPONENT SettingsAppearanceRuntime")
        if(NOT cmake_content MATCHES "${required}")
            message(FATAL_ERROR
                "Audio Settings package registry is incomplete: ${required}")
        endif()
    endforeach()
endif()

set(settings_main "${SOURCE_ROOT}/src/apps/settings_center/main.cpp")
if(EXISTS "${settings_main}")
    file(READ "${settings_main}" main_content)
    if(main_content MATCHES "services/audio_service/|wp/wp\\.h|QindaQt::Audio(Service)")
        message(FATAL_ERROR
            "Settings composition linked a private Audio1 implementation")
    endif()
endif()

list(LENGTH route_cpp cpp_count)
list(LENGTH route_qml qml_count)
if(EXISTS "${route_root}" AND cpp_count EQUAL 0)
    message(FATAL_ERROR "Audio Settings boundary found no C++ domain sources")
endif()
message(STATUS
    "Audio Settings remains a public-client-only route (${cpp_count} C++ and ${qml_count} QML files)")
