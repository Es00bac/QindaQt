# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)
project(QindaQtInstalledClipboardAppletConsumer LANGUAGES CXX)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

foreach(required IN ITEMS
        QINDAQT_STAGE_ROOT
        QINDAQT_STAGE_INCLUDE_DIR
        QINDAQT_STAGE_LIBDIR
        QINDAQT_STAGE_QML_ROOT
        QINDAQT_CONSUMER_SOURCE)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing staged-consumer input: ${required}")
    endif()
endforeach()

set(applet_module_dir
    "${QINDAQT_STAGE_QML_ROOT}/QindaQt/Shell/ClipboardApplet")
set(applet_lib_dir "${QINDAQT_STAGE_ROOT}/${QINDAQT_STAGE_LIBDIR}")

# Every QindaQt input is a staged file: the applet component's own archives
# and headers, plus the sibling artifacts the harness copied into the stage
# because their install components belong to other owners (AppShell
# precedent). Nothing here references the build or source tree.
add_executable(qindaqt_installed_clipboard_applet_consumer "${QINDAQT_CONSUMER_SOURCE}")
target_include_directories(
    qindaqt_installed_clipboard_applet_consumer
    PRIVATE "${QINDAQT_STAGE_INCLUDE_DIR}"
)
target_compile_features(qindaqt_installed_clipboard_applet_consumer PRIVATE cxx_std_20)
set_target_properties(
    qindaqt_installed_clipboard_applet_consumer
    PROPERTIES
        CXX_EXTENSIONS OFF
        # The RPATH row asserts these stay stage-relative: a relocated prefix
        # resolves Controls/Tokens without any build-tree path.
        BUILD_RPATH
            "${QINDAQT_STAGE_QML_ROOT}/QindaQt/Controls;${QINDAQT_STAGE_QML_ROOT}/QindaQt/Tokens"
)
target_compile_definitions(
    qindaqt_installed_clipboard_applet_consumer
    PRIVATE
        QINDAQT_STAGED_QML_ROOT="${QINDAQT_STAGE_QML_ROOT}"
        QINDAQT_STAGED_THEME_PATH="${QINDAQT_STAGE_ROOT}/data/themes/qinda-light.json"
        QINDAQT_STAGED_SURFACE_PATH="${applet_module_dir}/qml/ClipboardApplet.qml"
)

# Static QML plugin registration: the plugin archive must be linked whole so
# its registration translation unit survives --as-needed archive semantics.
target_link_libraries(
    qindaqt_installed_clipboard_applet_consumer
    PRIVATE
        "-Wl,--whole-archive"
        "${applet_module_dir}/libqindaqt_shell_clipboard_applet_runtimeplugin.a"
        "-Wl,--no-whole-archive"
        "${applet_module_dir}/libqindaqt_shell_clipboard_applet_runtime.a"
        "${applet_lib_dir}/libqindaqt_shell_clipboard_applet.a"
        "${applet_lib_dir}/libqindaqt_clipboard_model.a"
        "${applet_lib_dir}/libqindaqt_themes.a"
        "${applet_lib_dir}/libqindaqt_design_tokens.a"
        "${QINDAQT_STAGE_QML_ROOT}/QindaQt/Controls/libqindaqt_controls_qml.so"
        "${QINDAQT_STAGE_QML_ROOT}/QindaQt/Tokens/libqindaqt_tokens_qml.so"
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
