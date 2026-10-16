# SPDX-License-Identifier: GPL-3.0-or-later
#
# ADR-0384: the window model stays coherent when the world changes under it.
# Output-removal rescue, the container lifetime ledger and its dissolution
# paths, and off-thread foreign window identity. Its own file because
# tests/compositor/CMakeLists.txt is past its shape budget.

set(_qindaqt_window_model_kwin "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin")
set(_qindaqt_window_model_placement_sources
    "${_qindaqt_window_model_kwin}/hybridcontainerplacement.cpp"
    "${_qindaqt_window_model_kwin}/hybridcontainermaximize.cpp"
    "${_qindaqt_window_model_kwin}/hybridcontainershade.cpp"
    "${_qindaqt_window_model_kwin}/hybridcontainerrescue.cpp"
    "${_qindaqt_window_model_kwin}/hybridoutputrescue.cpp"
    "${_qindaqt_window_model_kwin}/hybridshadestripgeometry.cpp"
)
set(_qindaqt_window_model_placement_libraries
    QindaQt::Hybrid
    QindaQt::WindowManagement
    QindaQt::HybridChrome
    QindaQt::HybridConstraints
    QindaQt::HybridInput
    Qt6::Test
)

qt_add_executable(
    qindaqt_output_rescue_tests
    tst_outputrescue.cpp
    hybridcontainerplacement_fixture.h
    ${_qindaqt_window_model_placement_sources}
)
target_include_directories(
    qindaqt_output_rescue_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${_qindaqt_window_model_kwin}"
)
target_link_libraries(
    qindaqt_output_rescue_tests PRIVATE ${_qindaqt_window_model_placement_libraries}
)
add_test(NAME compositor.output-rescue COMMAND qindaqt_output_rescue_tests)

qt_add_executable(
    qindaqt_container_lifetime_tests
    tst_containerlifetime.cpp
    hybridinteractionruntime_testfixtures.h
    "${_qindaqt_window_model_kwin}/containerlifetimeledger.cpp"
    "${_qindaqt_window_model_kwin}/hybridcontainerappearance.cpp"
    "${_qindaqt_window_model_kwin}/hybridinteractionruntime.cpp"
    "${_qindaqt_window_model_kwin}/hybridinteractionplacement.cpp"
    ${_qindaqt_window_model_placement_sources}
)
target_include_directories(
    qindaqt_container_lifetime_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${_qindaqt_window_model_kwin}"
)
target_link_libraries(
    qindaqt_container_lifetime_tests
    PRIVATE
        ${_qindaqt_window_model_placement_libraries}
        QindaQt::CompositorShellActions
        QindaQt::WindowManagementGrouping
)
add_test(NAME compositor.container-lifetime COMMAND qindaqt_container_lifetime_tests)

qt_add_executable(
    qindaqt_foreign_identity_resolver_tests
    tst_foreignidentityresolver.cpp
    "${_qindaqt_window_model_kwin}/foreignidentityresolver.cpp"
)
target_include_directories(
    qindaqt_foreign_identity_resolver_tests PRIVATE "${_qindaqt_window_model_kwin}"
)
target_link_libraries(
    qindaqt_foreign_identity_resolver_tests
    PRIVATE QindaQt::CompositorShellActions Qt6::Test
)
add_test(
    NAME compositor.foreign-identity-resolver
    COMMAND qindaqt_foreign_identity_resolver_tests
)
set_tests_properties(
    compositor.foreign-identity-resolver
    PROPERTIES LABELS "unit;compositor;task-list" TIMEOUT 30
)

if(COMMAND qindaqt_enable_warnings)
    foreach(_qindaqt_window_model_target IN ITEMS
            qindaqt_output_rescue_tests
            qindaqt_container_lifetime_tests
            qindaqt_foreign_identity_resolver_tests)
        qindaqt_enable_warnings("${_qindaqt_window_model_target}")
    endforeach()
endif()
