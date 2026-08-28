# SPDX-License-Identifier: GPL-3.0-or-later

# This directory can configure independently while the parent integration remains
# a two-line add_subdirectory change in src/ and tests/.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtCompositorBridgeTests LANGUAGES CXX)
    include(CTest)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test)
    qt_standard_project_setup(REQUIRES 6.11)
    set(QINDAQT_BUILD_KWIN_PLUGIN OFF CACHE BOOL
        "Build only the compositor bridge in standalone tests" FORCE)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../src/core" core)
    add_subdirectory(
        "${CMAKE_CURRENT_LIST_DIR}/../../src/shell_visibility_protocol"
        shell_visibility_protocol
    )
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../src/compositor" compositor)
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

string(REPLACE ";" "\\;" _qindaqt_test_cmake_prefix_path "${CMAKE_PREFIX_PATH}")

qt_add_executable(qindaqt_compositor_codec_tests tst_controlcodec.cpp)
target_link_libraries(
    qindaqt_compositor_codec_tests
    PRIVATE
        QindaQt::CompositorBridge
        Qt6::Test
)
add_test(NAME compositor.control-codec COMMAND qindaqt_compositor_codec_tests)

qt_add_executable(qindaqt_compositor_bridge_tests tst_containercontrolbridge.cpp)
target_link_libraries(
    qindaqt_compositor_bridge_tests
    PRIVATE
        QindaQt::CompositorBridge
        Qt6::Test
)
add_test(NAME compositor.transaction-bridge COMMAND qindaqt_compositor_bridge_tests)

include("${CMAKE_CURRENT_LIST_DIR}/ShellVisibilityTests.cmake")

qt_add_executable(
    qindaqt_layout_geometry_tests
    tst_layoutgeometry.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/layoutgeometry.cpp"
)
target_include_directories(
    qindaqt_layout_geometry_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
)
target_link_libraries(qindaqt_layout_geometry_tests PRIVATE QindaQt::Core Qt6::Test)
add_test(NAME compositor.layout-geometry COMMAND qindaqt_layout_geometry_tests)

set(_qindaqt_kwin_abi_test_arguments
    -DQINDAQT_KWIN_ABI_MODULE=${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/cmake/QindaQtKWinAbi.cmake
    -DQINDAQT_KWIN_MANIFEST=${CMAKE_CURRENT_LIST_DIR}/../../compositor/upstream/kwin.json
)
add_test(
    NAME compositor.kwin-abi-pin
    COMMAND
        "${CMAKE_COMMAND}"
        ${_qindaqt_kwin_abi_test_arguments}
        -DQINDAQT_KWIN_ABI_VERSION:STRING=6.6.5
        -P "${CMAKE_CURRENT_LIST_DIR}/verify_kwin_abi_configuration.cmake"
)
add_test(
    NAME compositor.rejects-conflicting-kwin-abi
    COMMAND
        "${CMAKE_COMMAND}"
        ${_qindaqt_kwin_abi_test_arguments}
        -DQINDAQT_KWIN_ABI_VERSION:STRING=6.6.4
        -P "${CMAKE_CURRENT_LIST_DIR}/verify_kwin_abi_configuration.cmake"
)
set_tests_properties(
    compositor.rejects-conflicting-kwin-abi
    PROPERTIES WILL_FAIL TRUE
)

add_test(
    NAME compositor.kwin-plugin-dependency-contract
    COMMAND
        "${CMAKE_COMMAND}"
        -DQINDAQT_SOURCE_DIR=${CMAKE_CURRENT_LIST_DIR}/../..
        -DQINDAQT_TEST_BINARY_ROOT=${CMAKE_CURRENT_BINARY_DIR}/kwin-dependency-contract
        -DQINDAQT_TEST_GENERATOR=${CMAKE_GENERATOR}
        "-DQINDAQT_TEST_CMAKE_PREFIX_PATH=${_qindaqt_test_cmake_prefix_path}"
        -P "${CMAKE_CURRENT_LIST_DIR}/verify_kwin_plugin_dependency.cmake"
)
set_tests_properties(
    compositor.kwin-plugin-dependency-contract
    PROPERTIES TIMEOUT 60 LABELS "configuration;compositor"
)

add_test(
    NAME compositor.dbus-contract
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_LIST_DIR}/test_dbus_contract.py"
        "${CMAKE_CURRENT_LIST_DIR}/../../compositor/dbus/org.qindaqt.Compositor1.xml"
        "${CMAKE_CURRENT_LIST_DIR}/../../compositor/dbus/service.json"
)
set_tests_properties(
    compositor.dbus-contract
    PROPERTIES ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1" LABELS unit
)

if(TARGET qindaqt_kwin_input_adapter)
    find_package(KF6GlobalAccel 6.0 REQUIRED CONFIG)
    find_package(KDecoration3 6.6 REQUIRED CONFIG)
    find_package(KWin 6.6.5 EXACT REQUIRED CONFIG)
    qt_add_executable(
        qindaqt_kwin_input_adapter_tests
        tst_kwininputadapter.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/mutationcontrol.cpp"
    )
    target_include_directories(
        qindaqt_kwin_input_adapter_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
    )
    target_link_libraries(
        qindaqt_kwin_input_adapter_tests
        PRIVATE qindaqt_kwin_input_adapter Qt6::Test
    )
    add_test(NAME compositor.kwin-input-adapter COMMAND qindaqt_kwin_input_adapter_tests)

    qt_add_executable(
        qindaqt_kwin_output_inventory_tests
        tst_kwinoutputinventory.cpp
    )
    target_link_libraries(
        qindaqt_kwin_output_inventory_tests
        PRIVATE qindaqt_kwin_output_support Qt6::Test
    )
    add_test(
        NAME compositor.kwin-output-inventory
        COMMAND qindaqt_kwin_output_inventory_tests
    )

    qt_add_executable(
        qindaqt_development_output_protocol_tests
        tst_kwindevelopmentoutputseam.cpp
    )
    target_link_libraries(
        qindaqt_development_output_protocol_tests
        PRIVATE qindaqt_kwin_output_support Qt6::Test
    )
    add_test(
        NAME compositor.development-output-protocol
        COMMAND qindaqt_development_output_protocol_tests
    )

    qt_add_executable(
        qindaqt_hybrid_chrome_pointer_router_tests
        tst_hybridchromepointerrouter.cpp
    )
    target_include_directories(
        qindaqt_hybrid_chrome_pointer_router_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
    )
    target_link_libraries(
        qindaqt_hybrid_chrome_pointer_router_tests
        PRIVATE qindaqt_hybrid_chrome_pointer_router Qt6::Test
    )
    add_test(
        NAME compositor.hybrid-chrome-pointer-router
        COMMAND qindaqt_hybrid_chrome_pointer_router_tests
    )

    qt_add_executable(
        qindaqt_development_input_protocol_tests
        tst_developmentinputprotocol.cpp
    )
    target_link_libraries(
        qindaqt_development_input_protocol_tests
        PRIVATE qindaqt_kwin_development_input Qt6::Test
    )
    add_test(
        NAME compositor.development-input-protocol
        COMMAND qindaqt_development_input_protocol_tests
    )

    qt_add_executable(
        qindaqt_kwin_development_input_injector_tests
        tst_kwindevelopmentinputinjector.cpp
    )
    target_link_libraries(
        qindaqt_kwin_development_input_injector_tests
        PRIVATE qindaqt_kwin_development_input Qt6::Test
    )
    add_test(
        NAME compositor.kwin-development-input-injector
        COMMAND qindaqt_kwin_development_input_injector_tests
    )

    qt_add_executable(
        qindaqt_hybrid_shortcut_manager_tests
        tst_hybridshortcutmanager.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridshortcutmanager.cpp"
    )
    target_include_directories(
        qindaqt_hybrid_shortcut_manager_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
    )
    target_link_libraries(
        qindaqt_hybrid_shortcut_manager_tests
        PRIVATE KF6::GlobalAccel Qt6::Gui Qt6::Test
    )
    add_test(
        NAME compositor.hybrid-shortcut-manager
        COMMAND qindaqt_hybrid_shortcut_manager_tests
    )
    set_tests_properties(
        compositor.hybrid-shortcut-manager
        PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(
        qindaqt_kwin_chrome_manager_tests
        tst_kwinchromemanager.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridstackingorder.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/kwinchromehitrouting.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/kwinchromemanager.cpp"
    )
    target_include_directories(
        qindaqt_kwin_chrome_manager_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
    )
    target_link_libraries(
        qindaqt_kwin_chrome_manager_tests
        PRIVATE
            KWin::kwin
            QindaQt::Hybrid
            QindaQt::HybridChrome
            QindaQt::HybridInput
            qindaqt_hybrid_chrome_pointer_router
            Qt6::Widgets
            Qt6::Test
    )
    add_test(NAME compositor.kwin-chrome-manager COMMAND qindaqt_kwin_chrome_manager_tests)
    set_tests_properties(
        compositor.kwin-chrome-manager
        PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(
        qindaqt_kwin_chrome_scene_lifecycle_tests
        tst_kwinchromescenelifecycle.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/kwinchromescenelifecycle.cpp"
    )
    target_include_directories(
        qindaqt_kwin_chrome_scene_lifecycle_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
    )
    target_link_libraries(
        qindaqt_kwin_chrome_scene_lifecycle_tests
        PRIVATE Qt6::Core Qt6::Test
    )
    add_test(
        NAME compositor.kwin-chrome-scene-lifecycle
        COMMAND qindaqt_kwin_chrome_scene_lifecycle_tests
    )

    qt_add_executable(
        qindaqt_hybrid_group_stacking_tests
        tst_hybridgroupstacking.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridgroupstackingpolicy.cpp"
    )
    target_include_directories(
        qindaqt_hybrid_group_stacking_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
    )
    target_link_libraries(
        qindaqt_hybrid_group_stacking_tests
        PRIVATE Qt6::Core Qt6::Test
    )
    add_test(
        NAME compositor.hybrid-group-stacking
        COMMAND qindaqt_hybrid_group_stacking_tests
    )

    qt_add_executable(
        qindaqt_hybrid_chrome_sync_tests
        tst_hybridchromesync.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridchromesyncscheduler.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridstackingorder.cpp"
    )
    target_include_directories(
        qindaqt_hybrid_chrome_sync_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
    )
    target_link_libraries(
        qindaqt_hybrid_chrome_sync_tests
        PRIVATE Qt6::Core Qt6::Test
    )
    add_test(
        NAME compositor.hybrid-chrome-sync
        COMMAND qindaqt_hybrid_chrome_sync_tests
    )

    qt_add_executable(
        qindaqt_hybrid_chrome_plan_builder_tests
        tst_hybridchromeplanbuilder.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridchromeplanbuilder.cpp"
    )
    target_include_directories(
        qindaqt_hybrid_chrome_plan_builder_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
    )
    target_link_libraries(
        qindaqt_hybrid_chrome_plan_builder_tests
        PRIVATE
            QindaQt::Core
            QindaQt::HybridChrome
            QindaQt::HybridConstraints
            Qt6::Test
    )
    add_test(
        NAME compositor.hybrid-chrome-plan-builder
        COMMAND qindaqt_hybrid_chrome_plan_builder_tests
    )

    foreach(member_policy_test IN ITEMS hybridmemberpolicy hybridtransientpolicy)
        qt_add_executable(
            qindaqt_${member_policy_test}_tests
            "tst_${member_policy_test}.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/${member_policy_test}.cpp"
        )
        target_include_directories(
            qindaqt_${member_policy_test}_tests
            PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
        )
        target_link_libraries(
            qindaqt_${member_policy_test}_tests
            PRIVATE Qt6::Core Qt6::Test
        )
        add_test(
            NAME "compositor.${member_policy_test}"
            COMMAND qindaqt_${member_policy_test}_tests
        )
    endforeach()

    qt_add_executable(
        qindaqt_hybrid_chrome_drag_translator_tests
        tst_hybridchromedragtranslator.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridchromedragtranslator.cpp"
    )
    target_include_directories(
        qindaqt_hybrid_chrome_drag_translator_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
    )
    target_link_libraries(
        qindaqt_hybrid_chrome_drag_translator_tests
        PRIVATE
            QindaQt::Hybrid
            QindaQt::HybridChrome
            QindaQt::HybridInput
            Qt6::Test
    )
    add_test(
        NAME compositor.hybrid-chrome-drag-translator
        COMMAND qindaqt_hybrid_chrome_drag_translator_tests
    )

    qt_add_executable(
        qindaqt_hybrid_dock_target_routing_tests
        tst_hybriddocktargetrouting.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybriddocktargetrouting.cpp"
    )
    target_include_directories(
        qindaqt_hybrid_dock_target_routing_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
    )
    target_link_libraries(
        qindaqt_hybrid_dock_target_routing_tests
        PRIVATE QindaQt::HybridInput Qt6::Test
    )
    add_test(
        NAME compositor.hybrid-dock-target-routing
        COMMAND qindaqt_hybrid_dock_target_routing_tests
    )

    qt_add_executable(
        qindaqt_hybrid_chrome_exposure_tests
        tst_hybridchromeexposure.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridchromeexposure.cpp"
    )
    target_include_directories(
        qindaqt_hybrid_chrome_exposure_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
    )
    target_link_libraries(
        qindaqt_hybrid_chrome_exposure_tests
        PRIVATE Qt6::Core Qt6::Test
    )
    add_test(
        NAME compositor.hybrid-chrome-exposure
        COMMAND qindaqt_hybrid_chrome_exposure_tests
    )

    qt_add_executable(
        qindaqt_hybrid_group_context_tests
        tst_hybridgroupcontext.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridgroupcontext.cpp"
    )
    target_include_directories(
        qindaqt_hybrid_group_context_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
    )
    target_link_libraries(
        qindaqt_hybrid_group_context_tests
        PRIVATE Qt6::Core Qt6::Test
    )
    add_test(
        NAME compositor.hybrid-group-context
        COMMAND qindaqt_hybrid_group_context_tests
    )

    qt_add_executable(
        qindaqt_hybrid_window_admission_tests
        tst_hybridwindowadmission.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridwindowadmission.cpp"
    )
    target_include_directories(
        qindaqt_hybrid_window_admission_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
    )
    target_link_libraries(
        qindaqt_hybrid_window_admission_tests
        PRIVATE Qt6::Core Qt6::Test
    )
    add_test(
        NAME compositor.hybrid-window-admission
        COMMAND qindaqt_hybrid_window_admission_tests
    )

    qt_add_executable(
        qindaqt_hybrid_container_placement_tests
        tst_hybridcontainerplacement.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridcontainerplacement.cpp"
    )
    target_include_directories(
        qindaqt_hybrid_container_placement_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
    )
    target_link_libraries(
        qindaqt_hybrid_container_placement_tests
        PRIVATE
            QindaQt::Hybrid
            QindaQt::HybridChrome
            QindaQt::HybridConstraints
            QindaQt::HybridInput
            Qt6::Test
    )
    add_test(
        NAME compositor.hybrid-container-placement
        COMMAND qindaqt_hybrid_container_placement_tests
    )

    include("${CMAKE_CURRENT_LIST_DIR}/HybridSceneTests.cmake")

    qt_add_executable(
        qindaqt_kwin_dock_preview_tests
        tst_kwindockpreview.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/kwindockpreview.cpp"
    )
    target_include_directories(
        qindaqt_kwin_dock_preview_tests
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
    )
    target_link_libraries(
        qindaqt_kwin_dock_preview_tests
        PRIVATE QindaQt::HybridInput Qt6::Widgets Qt6::Test
    )
    add_test(NAME compositor.kwin-dock-preview COMMAND qindaqt_kwin_dock_preview_tests)
    set_tests_properties(
        compositor.kwin-dock-preview
        PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(
        qindaqt_container_close_prompt_tests
        tst_containercloseprompt.cpp
    )
    target_link_libraries(
        qindaqt_container_close_prompt_tests
        PRIVATE qindaqt_container_close_prompt Qt6::Widgets Qt6::Test
    )
    add_test(
        NAME compositor.container-close-prompt
        COMMAND qindaqt_container_close_prompt_tests
    )
    set_tests_properties(
        compositor.container-close-prompt
        PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    include("${CMAKE_CURRENT_LIST_DIR}/HybridContextMenuTest.cmake")

    foreach(runtime_test IN ITEMS
            hybridinteractionruntime
            hybridinteractiontranslation
            hybridinteractionpages)
        qt_add_executable(
            qindaqt_${runtime_test}_tests
            "tst_${runtime_test}.cpp"
            hybridinteractionruntime_testfixtures.h
            "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridinteractionruntime.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridinteractionplacement.cpp"
        )
        target_include_directories(
            qindaqt_${runtime_test}_tests
            PRIVATE
                "${CMAKE_CURRENT_LIST_DIR}"
                "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
        )
        target_link_libraries(
            qindaqt_${runtime_test}_tests
            PRIVATE QindaQt::Hybrid QindaQt::HybridInput Qt6::Test
        )
        add_test(
            NAME "compositor.${runtime_test}"
            COMMAND qindaqt_${runtime_test}_tests
        )
    endforeach()
endif()

include("${CMAKE_CURRENT_LIST_DIR}/TestWarnings.cmake")
