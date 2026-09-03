# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)
project(QindaQtInstalledTaskListAppletConsumer LANGUAGES CXX)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus Gui Qml Quick QuickControls2)

foreach(required IN ITEMS
        QINDAQT_STAGE_ROOT
        QINDAQT_STAGE_INCLUDE_DIR
        QINDAQT_STAGE_LIBDIR
        QINDAQT_STAGE_QML_ROOT
        QINDAQT_CONSUMER_SOURCE
        QINDAQT_STATIC_LIBRARY_SUFFIX)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing staged-consumer input: ${required}")
    endif()
endforeach()

set(applet_module_dir "${QINDAQT_STAGE_QML_ROOT}/QindaQt/Shell/TaskList")
set(applet_lib_dir "${QINDAQT_STAGE_ROOT}/${QINDAQT_STAGE_LIBDIR}")
set(static_suffix "${QINDAQT_STATIC_LIBRARY_SUFFIX}")

# Stage-relative layout: the consumer binary lives in <stage>/consumer-build,
# so every staged artifact is reachable through $ORIGIN/.. at runtime. The
# harness moves the whole stage and reruns the consumer WITHOUT
# LD_LIBRARY_PATH; any absolute RPATH or compile-time stage path breaks that
# relocation proof.
cmake_path(RELATIVE_PATH QINDAQT_STAGE_QML_ROOT
           BASE_DIRECTORY "${QINDAQT_STAGE_ROOT}"
           OUTPUT_VARIABLE qml_root_relative)
set(theme_relative "data/themes/qinda-light.json")
set(surface_relative
    "${qml_root_relative}/QindaQt/Shell/TaskList/qml/TaskListApplet.qml")

# Every QindaQt input is a staged file: the TaskListAppletRuntime component's
# own archives and headers, plus the sibling artifacts the harness copied into
# the stage because their install components belong to other owners (the T0/T1
# task-list archives, the themes/design-tokens archives, and the
# Controls/Tokens QML modules — clipboard/AppShell precedent). Nothing here
# references the build or source tree.
add_executable(qindaqt_installed_task_list_applet_consumer "${QINDAQT_CONSUMER_SOURCE}")
target_include_directories(
    qindaqt_installed_task_list_applet_consumer
    PRIVATE "${QINDAQT_STAGE_INCLUDE_DIR}"
)
target_compile_features(qindaqt_installed_task_list_applet_consumer PRIVATE cxx_std_20)
set_target_properties(
    qindaqt_installed_task_list_applet_consumer
    PROPERTIES
        CXX_EXTENSIONS OFF
        AUTOMOC OFF
        # AGENT-CONTRACT: the RPATH row asserts these stay $ORIGIN-relative —
        # a relocated prefix resolves the staged Controls/Tokens modules
        # without any build-tree or absolute stage path and without
        # LD_LIBRARY_PATH.
        BUILD_RPATH
            "$ORIGIN/../${qml_root_relative}/QindaQt/Controls;$ORIGIN/../${qml_root_relative}/QindaQt/Tokens"
)
target_compile_definitions(
    qindaqt_installed_task_list_applet_consumer
    PRIVATE
        QINDAQT_STAGED_QML_RELATIVE="${qml_root_relative}"
        QINDAQT_STAGED_THEME_RELATIVE="${theme_relative}"
        QINDAQT_STAGED_SURFACE_RELATIVE="${surface_relative}"
)

# Static QML plugin registration: the plugin archive must be linked whole so
# its registration translation unit survives --as-needed archive semantics.
# Archive order lists dependents before their dependencies for one-pass ld.
target_link_libraries(
    qindaqt_installed_task_list_applet_consumer
    PRIVATE
        "-Wl,--whole-archive"
        "${applet_module_dir}/libqindaqt_shell_task_list_appletplugin${static_suffix}"
        "-Wl,--no-whole-archive"
        "${applet_module_dir}/libqindaqt_shell_task_list_applet${static_suffix}"
        "${applet_lib_dir}/libqindaqt_shell_task_list_operations${static_suffix}"
        "${applet_lib_dir}/libqindaqt_shell_task_list_producer${static_suffix}"
        "${applet_lib_dir}/libqindaqt_shell_task_list${static_suffix}"
        "${applet_lib_dir}/libqindaqt_themes${static_suffix}"
        "${applet_lib_dir}/libqindaqt_design_tokens${static_suffix}"
        "${QINDAQT_STAGE_QML_ROOT}/QindaQt/Controls/libqindaqt_controls_qml${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "${QINDAQT_STAGE_QML_ROOT}/QindaQt/Tokens/libqindaqt_tokens_qml${CMAKE_SHARED_LIBRARY_SUFFIX}"
        # The staged T1 producer archive references Qt D-Bus symbols; the
        # consumer never opens a bus connection, it only links the library.
        Qt6::DBus
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
