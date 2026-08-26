# SPDX-License-Identifier: GPL-3.0-or-later

# This directory can configure independently while the parent integration remains
# a two-line add_subdirectory change in src/ and tests/.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtCompositorBridgeTests LANGUAGES CXX)
    include(CTest)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test)
    qt_standard_project_setup(REQUIRES 6.11)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../src/core" core)
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
endif()

if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(qindaqt_compositor_codec_tests)
    qindaqt_enable_warnings(qindaqt_compositor_bridge_tests)
    qindaqt_enable_warnings(qindaqt_layout_geometry_tests)
    if(TARGET qindaqt_kwin_input_adapter_tests)
        qindaqt_enable_warnings(qindaqt_kwin_input_adapter_tests)
    endif()
endif()
