# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS BINARY_ROOT POISON_ROOT CHECK_SCRIPT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing Audio Settings poison input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH BINARY_ROOT OUTPUT_VARIABLE binary_root)
cmake_path(NORMAL_PATH POISON_ROOT OUTPUT_VARIABLE poison_root)
cmake_path(IS_PREFIX binary_root "${poison_root}" NORMALIZE poison_in_build)
if(NOT poison_in_build OR poison_root EQUAL binary_root)
    message(FATAL_ERROR "Audio Settings poison root must stay below the build tree")
endif()

function(expect_rejection relative_path source_text expected)
    file(REMOVE_RECURSE "${poison_root}")
    cmake_path(GET relative_path PARENT_PATH parent)
    file(MAKE_DIRECTORY "${poison_root}/${parent}")
    file(WRITE "${poison_root}/${relative_path}" "${source_text}\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${poison_root}"
                -P "${CHECK_SCRIPT}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(status EQUAL 0)
        message(FATAL_ERROR
            "Audio Settings boundary accepted poison ${relative_path}")
    endif()
    string(CONCAT combined "${output}" "${error}")
    if(NOT combined MATCHES "${expected}")
        message(FATAL_ERROR
            "Audio Settings poison was rejected for the wrong reason:\n${combined}")
    endif()
endfunction()

function(expect_permitted_surface)
    file(REMOVE_RECURSE "${poison_root}")
    file(MAKE_DIRECTORY "${poison_root}/src/apps/settings/audio/qml")
    file(WRITE "${poison_root}/src/apps/settings/audio/allowed.h"
        "class Allowed {\n"
        "  Q_INVOKABLE bool reload();\n"
        "  Q_INVOKABLE bool setDefaultDevice(quint64 serial);\n"
        "  Q_INVOKABLE bool setDeviceVolume(quint64 serial, double level);\n"
        "  Q_INVOKABLE bool setDeviceMuted(quint64 serial, bool muted);\n"
        "  Q_INVOKABLE bool setStreamVolume(quint64 serial, double level);\n"
        "  Q_INVOKABLE bool setStreamMuted(quint64 serial, bool muted);\n"
        "};\n")
    file(WRITE "${poison_root}/src/apps/settings/audio/qml/Allowed.qml"
        "import QindaQt.Controls 1.0\nButton { text: \"Permitted\" }\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${poison_root}"
                -P "${CHECK_SCRIPT}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT status EQUAL 0)
        message(FATAL_ERROR
            "Audio Settings boundary rejected its permitted closed surface:\n${output}${error}")
    endif()
endfunction()

expect_permitted_surface()

expect_rejection(
    "src/apps/settings/audio/poison.cpp"
    "#include <qindaqt/services/audio_service/audio_service.h>"
    "crossed the public AudioClient boundary"
)
expect_rejection(
    "src/apps/settings/audio/poison.cpp"
    "#include <QtDBus/QDBusConnection>"
    "crossed the public AudioClient boundary"
)
expect_rejection(
    "src/apps/settings/audio/poison.cpp"
    "#include <wp/wp.h>"
    "crossed the public AudioClient boundary"
)
expect_rejection(
    "src/apps/settings/audio/poison.h"
    "class Poison { Q_INVOKABLE void moveStream(quint64, quint64); };"
    "invokable outside its closed intent surface"
)
expect_rejection(
    "src/apps/settings/audio/poison.h"
    "class Poison { Q_INVOKABLE void setBalance(double); };"
    "invokable outside its closed intent surface"
)
expect_rejection(
    "src/apps/settings/audio/qml/Poison.qml"
    "import QindaQt.Controls 1.0\nTextField { objectName: \"audioProfile\" }"
    "gained text entry"
)
expect_rejection(
    "src/apps/settings/audio/qml/Poison.qml"
    "import QtQuick\nTextInput { }"
    "gained text entry"
)
expect_rejection(
    "src/apps/settings/audio/qml/Poison.qml"
    "import QtQuick\nTextEdit { }"
    "gained text entry"
)
expect_rejection(
    "src/apps/settings/audio/qml/Poison.qml"
    "import QtQuick.Controls as T\nT.TextArea { }"
    "gained text entry"
)
expect_rejection(
    "src/apps/settings/audio/CMakeLists.txt"
    "target_link_libraries(poison PRIVATE QindaQt::AudioService)"
    "domain target reversed"
)
expect_rejection(
    "src/apps/settings/audio/CMakeLists.txt"
    "target_link_libraries(poison PRIVATE QindaQt::QtAudioTransport)"
    "domain target reversed"
)

file(REMOVE_RECURSE "${poison_root}")
message(STATUS "Audio Settings boundary rejected all injected poisons")
