# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)
project(QindaQtInstalledStatusNotifierAppletConsumer LANGUAGES CXX)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus Gui Qml Quick QuickControls2 Svg)

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
    "${QINDAQT_STAGE_QML_ROOT}/QindaQt/Shell/StatusNotifier")
set(icons_module_dir "${QINDAQT_STAGE_QML_ROOT}/QindaQt/Shell/Icons")
set(applet_lib_dir "${QINDAQT_STAGE_ROOT}/${QINDAQT_STAGE_LIBDIR}")

# Stage-relative layout: the consumer binary lives in <stage>/consumer-build,
# so every staged artifact is reachable through $ORIGIN/.. at runtime. The
# harness moves the whole stage and reruns the consumer WITHOUT
# LD_LIBRARY_PATH; absolute RPATH or compile-time stage paths would break that
# relocation proof.
cmake_path(RELATIVE_PATH QINDAQT_STAGE_QML_ROOT
           BASE_DIRECTORY "${QINDAQT_STAGE_ROOT}"
           OUTPUT_VARIABLE qml_root_relative)
set(theme_relative "data/themes/qinda-light.json")
set(surface_relative "${qml_root_relative}/QindaQt/Shell/StatusNotifier/qml/StatusNotifierApplet.qml")

# Every QindaQt input is a staged file: the applet component's own archives
# and headers, plus the sibling artifacts the harness copied into the stage
# because their install components belong to other owners (AppShell
# precedent). Nothing here references the build or source tree.
add_executable(qindaqt_installed_status_notifier_applet_consumer "${QINDAQT_CONSUMER_SOURCE}")
target_include_directories(
    qindaqt_installed_status_notifier_applet_consumer
    PRIVATE "${QINDAQT_STAGE_INCLUDE_DIR}"
)
target_compile_features(qindaqt_installed_status_notifier_applet_consumer PRIVATE cxx_std_20)
set_target_properties(
    qindaqt_installed_status_notifier_applet_consumer
    PROPERTIES
        CXX_EXTENSIONS OFF
        # AGENT-CONTRACT: the RPATH row asserts these stay $ORIGIN-relative —
        # a relocated prefix resolves the staged Controls/Tokens modules
        # without any build-tree or absolute stage path and without
        # LD_LIBRARY_PATH.
        BUILD_RPATH
            "$ORIGIN/../${qml_root_relative}/QindaQt/Controls;$ORIGIN/../${qml_root_relative}/QindaQt/Tokens"
)
target_compile_definitions(
    qindaqt_installed_status_notifier_applet_consumer
    PRIVATE
        QINDAQT_STAGED_QML_RELATIVE="${qml_root_relative}"
        QINDAQT_STAGED_THEME_RELATIVE="${theme_relative}"
        QINDAQT_STAGED_SURFACE_RELATIVE="${surface_relative}"
)

# Static QML plugin registration: the plugin archive must be linked whole so
# its registration translation unit survives --as-needed archive semantics.
target_link_libraries(
    qindaqt_installed_status_notifier_applet_consumer
    PRIVATE
        "-Wl,--whole-archive"
        "${applet_module_dir}/libqindaqt_shell_status_notifier_applet_runtimeplugin.a"
        "${icons_module_dir}/libqindaqt_shell_iconsplugin.a"
        "-Wl,--no-whole-archive"
        "${applet_module_dir}/libqindaqt_shell_status_notifier_applet_runtime.a"
        "${icons_module_dir}/libqindaqt_shell_icons.a"
        "${applet_lib_dir}/libqindaqt_shell_status_notifier_applet.a"
        "${applet_lib_dir}/libqindaqt_status_notifier.a"
        "${applet_lib_dir}/libqindaqt_status_notifier_item_client.a"
        "${applet_lib_dir}/libqindaqt_global_menu_dbusmenu.a"
        "${applet_lib_dir}/libqindaqt_global_menu_protocol.a"
        "${applet_lib_dir}/libqindaqt_status_notifier_icon.a"
        "${applet_lib_dir}/libqindaqt_themes.a"
        "${applet_lib_dir}/libqindaqt_design_tokens.a"
        "${QINDAQT_STAGE_QML_ROOT}/QindaQt/Controls/libqindaqt_controls_qml.so"
        "${QINDAQT_STAGE_QML_ROOT}/QindaQt/Tokens/libqindaqt_tokens_qml.so"
        # The staged S1 item-client archive carries QtDBus wire decoding; the
        # consumer links DBus for those symbols even though the applet itself
        # never opens a connection.
        Qt6::DBus
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Svg
)
