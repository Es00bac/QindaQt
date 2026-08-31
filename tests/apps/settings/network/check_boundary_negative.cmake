# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS BINARY_ROOT POISON_ROOT CHECK_SCRIPT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing Network Settings poison input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH BINARY_ROOT OUTPUT_VARIABLE binary_root)
cmake_path(NORMAL_PATH POISON_ROOT OUTPUT_VARIABLE poison_root)
cmake_path(IS_PREFIX binary_root "${poison_root}" NORMALIZE poison_in_build)
if(NOT poison_in_build OR poison_root STREQUAL binary_root)
    message(FATAL_ERROR "Network Settings poison root must stay below the build tree")
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
            "Network Settings boundary accepted poison ${relative_path}")
    endif()
    string(CONCAT combined "${output}" "${error}")
    if(NOT combined MATCHES "${expected}")
        message(FATAL_ERROR
            "Network Settings poison was rejected for the wrong reason:\n${combined}")
    endif()
endfunction()

function(expect_permitted_surface)
    file(REMOVE_RECURSE "${poison_root}")
    file(MAKE_DIRECTORY "${poison_root}/src/apps/settings/network/qml")
    file(WRITE "${poison_root}/src/apps/settings/network/allowed.h"
        "class Allowed {\n"
        "  Q_INVOKABLE bool reload();\n"
        "  Q_INVOKABLE bool requestScan();\n"
        "  Q_INVOKABLE bool connectKnownNetwork(const QString &knownNetworkId);\n"
        "  Q_INVOKABLE bool disconnectDevice(const QString &deviceInterface);\n"
        "};\n")
    file(WRITE "${poison_root}/src/apps/settings/network/qml/Allowed.qml"
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
            "Network Settings boundary rejected its permitted closed surface:\n${output}${error}")
    endif()
endfunction()

expect_permitted_surface()

expect_rejection(
    "src/apps/settings/network/poison.cpp"
    "#include <qindaqt/services/network_service/resident_network_service.h>"
    "crossed the public NetworkClient boundary"
)
expect_rejection(
    "src/apps/settings/network/poison.cpp"
    "#include <QtDBus/QDBusConnection>"
    "crossed the public NetworkClient boundary"
)
expect_rejection(
    "src/apps/settings/network/poison.h"
    "class Poison { Q_INVOKABLE void setRadio(bool); };"
    "invokable outside its closed intent surface"
)
expect_rejection(
    "src/apps/settings/network/qml/Poison.qml"
    "import QindaQt.Controls 1.0\nTextField { objectName: \"networkPassword\" }"
    "gained credential/profile/radio editing"
)
expect_rejection(
    "src/apps/settings/network/qml/Poison.qml"
    "import QtQuick\nTextInput { }"
    "gained credential/profile/radio editing"
)
expect_rejection(
    "src/apps/settings/network/qml/Poison.qml"
    "import QtQuick\nTextEdit { }"
    "gained credential/profile/radio editing"
)
expect_rejection(
    "src/apps/settings/network/qml/Poison.qml"
    "import QtQuick.Controls as T\nT.TextArea { }"
    "gained credential/profile/radio editing"
)
expect_rejection(
    "src/apps/settings/network/poison.h"
    "class Poison { Q_INVOKABLE void enableWifiRadio(bool); };"
    "invokable outside its closed intent surface"
)
expect_rejection(
    "src/apps/settings/network/poison.h"
    "class Poison { Q_INVOKABLE void submitKey(QString); };"
    "invokable outside its closed intent surface"
)
expect_rejection(
    "src/apps/settings/network/CMakeLists.txt"
    "target_link_libraries(poison PRIVATE QindaQt::NetworkService)"
    "domain target reversed"
)

file(REMOVE_RECURSE "${poison_root}")
message(STATUS "Network Settings boundary rejected all injected poisons")
