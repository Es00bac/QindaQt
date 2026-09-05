# SPDX-License-Identifier: GPL-3.0-or-later

# AGENT-NOTE: The root test registry owns this focused seam. The pure model
# rows link only the pure projection target; the controller and QML rows link
# the runtime target so fail-closed presentation behavior remains testable
# without the resident Audio1 service.
find_package(Qt6 6.11 REQUIRED COMPONENTS Core Quick Svg Test)

qt_add_executable(qindaqt_audio_applet_model_tests tst_audio_applet_model.cpp)
target_link_libraries(
    qindaqt_audio_applet_model_tests
    PRIVATE QindaQt::ShellAudioApplet Qt6::Test
)
qindaqt_enable_warnings(qindaqt_audio_applet_model_tests)
add_test(
    NAME qindaqt.audio-applet-model
    COMMAND qindaqt_audio_applet_model_tests
)
set_tests_properties(
    qindaqt.audio-applet-model PROPERTIES LABELS "shell;audio;pure"
)

qt_add_executable(
    qindaqt_audio_applet_controller_tests
    tst_audio_applet_controller.cpp
)
target_include_directories(
    qindaqt_audio_applet_controller_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../services/audio_client"
)
target_link_libraries(
    qindaqt_audio_applet_controller_tests
    PRIVATE QindaQt::ShellAudioAppletRuntime Qt6::Test
)
qindaqt_enable_warnings(qindaqt_audio_applet_controller_tests)
add_test(
    NAME qindaqt.audio-applet-controller
    COMMAND qindaqt_audio_applet_controller_tests
)
set_tests_properties(
    qindaqt.audio-applet-controller PROPERTIES LABELS "shell;audio;runtime"
)

qt_add_executable(qindaqt_audio_applet_qml_tests tst_audio_applet_qml.cpp)
target_include_directories(
    qindaqt_audio_applet_qml_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../services/audio_client"
)
target_link_libraries(
    qindaqt_audio_applet_qml_tests
    PRIVATE
        QindaQt::ShellAudioAppletRuntime
        QindaQt::TokensQml
        QindaQt::ShellIcons
        qindaqt_shell_iconsplugin
        qindaqt_shell_audio_applet_runtimeplugin
        qindaqt_themes
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
)
target_compile_definitions(
    qindaqt_audio_applet_qml_tests
    PRIVATE
        QINDAQT_AUDIO_APPLET_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
        QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
add_dependencies(
    qindaqt_audio_applet_qml_tests
    qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin
)
qindaqt_enable_warnings(qindaqt_audio_applet_qml_tests)
qt_import_qml_plugins(qindaqt_audio_applet_qml_tests)
add_test(
    NAME qindaqt.audio-applet-offscreen
    COMMAND qindaqt_audio_applet_qml_tests
)
set_tests_properties(
    qindaqt.audio-applet-offscreen
    PROPERTIES
        LABELS "shell;audio;runtime;offscreen;accessibility"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
)

add_test(
    NAME qindaqt.audio-applet-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        "-DPOISON_ROOT=${CMAKE_CURRENT_BINARY_DIR}/boundary-poison"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(
    qindaqt.audio-applet-boundary
    PROPERTIES LABELS "shell;audio;pure;policy;hostile"
)

add_test(
    NAME qindaqt.audio-applet-runtime-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        "-DPOISON_ROOT=${CMAKE_CURRENT_BINARY_DIR}/runtime-boundary-poison"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_runtime_boundary.cmake"
)
set_tests_properties(
    qindaqt.audio-applet-runtime-boundary
    PROPERTIES LABELS "shell;audio;runtime;policy;hostile"
)

if(TARGET qindaqt-shell)
    find_package(KF6GlobalAccel 6.0 REQUIRED CONFIG)
    add_test(
        NAME qindaqt.audio-applet-installed-package
        COMMAND
            "${CMAKE_COMMAND}"
            "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
            "-DQINDAQT_BUILD_ROOT=${CMAKE_BINARY_DIR}"
            "-DQINDAQT_STAGE=${CMAKE_CURRENT_BINARY_DIR}/installed-audio-applet"
            "-DQINDAQT_INSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}"
            "-DQINDAQT_INSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}"
            "-DQINDAQT_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
            "-DQINDAQT_KF6_GLOBALACCEL_LIBRARY=$<TARGET_FILE:KF6::GlobalAccel>"
            "-DQINDAQT_KF6_GLOBALACCEL_SONAME=$<TARGET_SONAME_FILE_NAME:KF6::GlobalAccel>"
            "-DQINDAQT_TOKENS_LIBRARY=$<TARGET_FILE:qindaqt_tokens_qml>"
            "-DQINDAQT_TOKENS_SONAME=$<TARGET_SONAME_FILE_NAME:qindaqt_tokens_qml>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_audio_applet.cmake"
    )
    set_tests_properties(
        qindaqt.audio-applet-installed-package
        PROPERTIES LABELS "shell;audio;runtime;installed;poison" TIMEOUT 60
    )

    add_test(
        NAME qindaqt.shell-runtime-component-closure
        COMMAND
            "${CMAKE_COMMAND}"
            "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
            "-DQINDAQT_SHELL_BUILD_ROOT=${CMAKE_BINARY_DIR}/src/shell"
            "-DQINDAQT_SHELL_CMAKE=${PROJECT_SOURCE_DIR}/src/shell/CMakeLists.txt"
            "-DQINDAQT_SHELL_INSTALL_DIR=${PROJECT_SOURCE_DIR}/src/shell"
            "-DQINDAQT_STAGE_ROOT=${CMAKE_CURRENT_BINARY_DIR}/installed-shell-components"
            "-DQINDAQT_INSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}"
            "-DQINDAQT_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_shell_component_closure.cmake"
    )
    set_tests_properties(
        qindaqt.shell-runtime-component-closure
        PROPERTIES LABELS "shell;runtime;installed;hostile" TIMEOUT 60
    )
endif()
