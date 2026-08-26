# SPDX-License-Identifier: GPL-3.0-or-later

# Cohesive scene/lifecycle targets live here so the compositor test entrypoint
# remains a readable registry rather than another 500-line integration file.
set(_qindaqt_kwin_hybrid_scene_common_sources
    kwinhybridscene_testfixture.h
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridgroupcontext.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridtaskidentitypolicy.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridwindowadmission.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/kwinhybridplatform.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/kwinhybridreflow.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/kwinhybridscene.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/managedwindowregistry.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/managedwindowregistry.h"
)

function(qindaqt_add_kwin_hybrid_scene_test target source test_name)
    qt_add_executable(
        "${target}"
        "${source}"
        ${_qindaqt_kwin_hybrid_scene_common_sources}
        ${ARGN}
    )
    target_include_directories(
        "${target}"
        PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}"
            "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
    )
    target_link_libraries(
        "${target}"
        PRIVATE
            QindaQt::Hybrid
            QindaQt::HybridConstraints
            QindaQt::HybridInput
            KDecoration3::KDecoration
            KWin::kwin
            Qt6::Test
    )
    add_test(NAME "${test_name}" COMMAND "${target}")
    list(APPEND _qindaqt_hybrid_scene_test_targets "${target}")
    set(_qindaqt_hybrid_scene_test_targets
        "${_qindaqt_hybrid_scene_test_targets}" PARENT_SCOPE)
endfunction()

qindaqt_add_kwin_hybrid_scene_test(
    qindaqt_kwin_hybrid_scene_tests
    tst_kwinhybridscene.cpp
    compositor.kwin-hybrid-scene
)
qindaqt_add_kwin_hybrid_scene_test(
    qindaqt_kwin_hybrid_pages_tests
    tst_kwinhybridpages.cpp
    compositor.kwin-hybrid-pages
)
qindaqt_add_kwin_hybrid_scene_test(
    qindaqt_kwin_hybrid_focus_tests
    tst_kwinhybridfocus.cpp
    compositor.kwin-hybrid-focus
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridinteractionruntime.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridinteractionplacement.cpp"
)
qindaqt_add_kwin_hybrid_scene_test(
    qindaqt_kwin_hybrid_context_tests
    tst_kwinhybridcontext.cpp
    compositor.kwin-hybrid-context
)
qindaqt_add_kwin_hybrid_scene_test(
    qindaqt_kwin_hybrid_task_identity_tests
    tst_kwinhybridtaskidentity.cpp
    compositor.kwin-hybrid-task-identity
)
qindaqt_add_kwin_hybrid_scene_test(
    qindaqt_kwin_hybrid_recovery_tests
    tst_kwinhybridrecovery.cpp
    compositor.kwin-hybrid-recovery
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/kwinhybridrecovery.cpp"
)

qt_add_executable(
    qindaqt_hybrid_task_identity_policy_tests
    tst_hybridtaskidentitypolicy.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridtaskidentitypolicy.cpp"
)
target_include_directories(
    qindaqt_hybrid_task_identity_policy_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
)
target_link_libraries(
    qindaqt_hybrid_task_identity_policy_tests
    PRIVATE QindaQt::Hybrid Qt6::Test
)
add_test(
    NAME compositor.hybrid-task-identity-policy
    COMMAND qindaqt_hybrid_task_identity_policy_tests
)

qt_add_executable(
    qindaqt_hybrid_shutdown_recovery_tests
    tst_hybridshutdownrecovery.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridshutdownrecovery.cpp"
)
target_include_directories(
    qindaqt_hybrid_shutdown_recovery_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
)
target_link_libraries(
    qindaqt_hybrid_shutdown_recovery_tests
    PRIVATE Qt6::Core Qt6::Test
)
add_test(
    NAME compositor.hybrid-shutdown-recovery
    COMMAND qindaqt_hybrid_shutdown_recovery_tests
)

qt_add_executable(
    qindaqt_hybrid_semantic_command_tests
    tst_hybridsemanticcommand.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridsemanticcommand.cpp"
)
target_include_directories(
    qindaqt_hybrid_semantic_command_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
)
target_link_libraries(
    qindaqt_hybrid_semantic_command_tests
    PRIVATE QindaQt::Hybrid QindaQt::HybridChrome QindaQt::HybridInput Qt6::Test
)
add_test(
    NAME compositor.hybrid-semantic-command
    COMMAND qindaqt_hybrid_semantic_command_tests
)

qt_add_executable(
    qindaqt_hybrid_chrome_accessibility_tests
    tst_hybridchromeaccessibility.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridchromeaccessibility.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridchromeaccessibility_p.h"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridchromeaccessibilitymodel.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridchromeaccessibleinterface.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridchromeaccessibilityregistry.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridsemanticcommand.cpp"
)
target_include_directories(
    qindaqt_hybrid_chrome_accessibility_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
)
target_link_libraries(
    qindaqt_hybrid_chrome_accessibility_tests
    PRIVATE
        QindaQt::Hybrid
        QindaQt::HybridChrome
        QindaQt::HybridInput
        Qt6::Widgets
        Qt6::Test
)
add_test(
    NAME compositor.hybrid-chrome-accessibility
    COMMAND qindaqt_hybrid_chrome_accessibility_tests
)
set_tests_properties(
    compositor.hybrid-chrome-accessibility
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

list(APPEND _qindaqt_hybrid_scene_test_targets
    qindaqt_hybrid_task_identity_policy_tests
    qindaqt_hybrid_shutdown_recovery_tests
    qindaqt_hybrid_semantic_command_tests
    qindaqt_hybrid_chrome_accessibility_tests
)
