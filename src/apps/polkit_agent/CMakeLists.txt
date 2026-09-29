# SPDX-License-Identifier: GPL-3.0-or-later
# qindaqt-polkit-agent: the QindaQt session's own polkit authentication
# agent (ADR-0290), replacing the KDE-agent-vs-polkit-gnome race ADR-0100
# left unresolved. Its dialog is a layer-shell overlay, so this whole app is
# skipped where the production Wayland shell is not being built (see the
# add_subdirectory guard in src/CMakeLists.txt).

find_package(PolkitQt6-1 REQUIRED)
find_package(LayerShellQt 6.6.6 REQUIRED)

# The retry/cancel state machine, identity and requester logic, and the QML
# view model: no polkit or Wayland type anywhere here, so the unit rows
# exercise exactly what the dialog reads and emits.
qt_add_library(qindaqt_polkit_agent_core STATIC)
target_sources(
    qindaqt_polkit_agent_core
    PRIVATE
        agent_identity.cpp
        agent_identity.h
        polkit_attempt_controller.cpp
        polkit_attempt_controller.h
        polkit_attempt_port.h
        polkit_dialog_view_model.cpp
        polkit_dialog_view_model.h
        polkit_request_queue.cpp
        polkit_request_queue.h
        polkit_requester_resolver.cpp
        polkit_requester_resolver.h
)
target_include_directories(qindaqt_polkit_agent_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qindaqt_polkit_agent_core PUBLIC Qt6::Core)
target_compile_features(qindaqt_polkit_agent_core PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_polkit_agent_core PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_polkit_agent_core)

# The appearance bootstrap is its own small target (not part of the core
# logic library above, and not embedded directly in the executable) purely
# so the offscreen QML test can link it too, exactly as it links the core
# logic library -- see tests/tst_polkit_dialog_qml.cpp's initTestCase().
qt_add_library(qindaqt_polkit_agent_appearance STATIC)
target_sources(
    qindaqt_polkit_agent_appearance
    PRIVATE polkit_agent_appearance.cpp polkit_agent_appearance.h
)
target_include_directories(qindaqt_polkit_agent_appearance PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(
    qindaqt_polkit_agent_appearance
    PUBLIC QindaQt::AppAppearance QindaQt::DesignTokens Qt6::Core Qt6::Gui Qt6::Qml
)
target_compile_features(qindaqt_polkit_agent_appearance PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_polkit_agent_appearance PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_polkit_agent_appearance)

qt_add_executable(
    qindaqt-polkit-agent
    main.cpp
    polkit_listener.cpp
    polkit_listener.h
    polkit_overlay_surface.cpp
    polkit_overlay_surface.h
)
qt_add_qml_module(
    qindaqt-polkit-agent
    URI QindaQt.PolkitAgentApp
    VERSION 1.0
    QML_FILES
        Main.qml
        PolkitDialogContent.qml
)
target_link_libraries(
    qindaqt-polkit-agent
    PRIVATE
        qindaqt_polkit_agent_core
        qindaqt_polkit_agent_appearance
        PolkitQt6-1::Core
        PolkitQt6-1::Agent
        LayerShellQt::Interface
        QindaQt::SettingsClient
        QindaQt::ShellIcons
        QindaQt::TokensQml
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
add_dependencies(qindaqt-polkit-agent qindaqt_tokens_qmlplugin qindaqt_controls_qmlplugin)
if(TARGET qindaqt_shell_iconsplugin)
    add_dependencies(qindaqt-polkit-agent qindaqt_shell_iconsplugin)
endif()
target_compile_definitions(
    qindaqt-polkit-agent
    PRIVATE
        QINDAQT_BUILD_EXECUTABLE_PATH="$<TARGET_FILE:qindaqt-polkit-agent>"
        QINDAQT_BUILD_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
        QINDAQT_INSTALL_QML_RELATIVE_PATH="../${QT6_INSTALL_QML}"
)
target_compile_features(qindaqt-polkit-agent PRIVATE cxx_std_20)
set_target_properties(
    qindaqt-polkit-agent PROPERTIES
        CXX_EXTENSIONS OFF
        INSTALL_RPATH "$ORIGIN/../${QT6_INSTALL_QML}/QindaQt/Tokens"
)
qindaqt_enable_warnings(qindaqt-polkit-agent)

# AGENT-CONTRACT: session_supervisor's defaultPolkitAgentCandidates()
# (src/session_supervisor/src/polkit_agent_selection.cpp) hardcodes this
# exact basename under KDE_INSTALL_FULL_LIBEXECDIR; the two are not wired
# together by CMake and must be kept in sync by hand.
install(
    TARGETS qindaqt-polkit-agent
    RUNTIME DESTINATION "${KDE_INSTALL_LIBEXECDIR}"
    COMPONENT PolkitAgent
)

if(BUILD_TESTING)
    qt_add_executable(qindaqt_agent_identity_tests tests/tst_agent_identity.cpp)
    target_link_libraries(qindaqt_agent_identity_tests PRIVATE qindaqt_polkit_agent_core Qt6::Test)
    qindaqt_enable_warnings(qindaqt_agent_identity_tests)
    add_test(NAME qindaqt.polkit-agent-identity COMMAND qindaqt_agent_identity_tests)
    set_tests_properties(qindaqt.polkit-agent-identity PROPERTIES LABELS "apps;polkit;model;pure")

    qt_add_executable(
        qindaqt_polkit_attempt_controller_tests tests/tst_polkit_attempt_controller.cpp
    )
    target_link_libraries(
        qindaqt_polkit_attempt_controller_tests PRIVATE qindaqt_polkit_agent_core Qt6::Test
    )
    qindaqt_enable_warnings(qindaqt_polkit_attempt_controller_tests)
    add_test(
        NAME qindaqt.polkit-agent-attempt-controller
        COMMAND qindaqt_polkit_attempt_controller_tests
    )
    set_tests_properties(
        qindaqt.polkit-agent-attempt-controller PROPERTIES LABELS "apps;polkit;model;pure"
    )

    qt_add_executable(qindaqt_polkit_request_queue_tests tests/tst_polkit_request_queue.cpp)
    target_link_libraries(
        qindaqt_polkit_request_queue_tests PRIVATE qindaqt_polkit_agent_core Qt6::Test
    )
    qindaqt_enable_warnings(qindaqt_polkit_request_queue_tests)
    add_test(NAME qindaqt.polkit-agent-request-queue COMMAND qindaqt_polkit_request_queue_tests)
    set_tests_properties(qindaqt.polkit-agent-request-queue PROPERTIES LABELS "apps;polkit;model;pure")

    qt_add_executable(
        qindaqt_polkit_requester_resolver_tests tests/tst_polkit_requester_resolver.cpp
    )
    target_link_libraries(
        qindaqt_polkit_requester_resolver_tests PRIVATE qindaqt_polkit_agent_core Qt6::Test
    )
    qindaqt_enable_warnings(qindaqt_polkit_requester_resolver_tests)
    add_test(
        NAME qindaqt.polkit-agent-requester-resolver
        COMMAND qindaqt_polkit_requester_resolver_tests
    )
    set_tests_properties(
        qindaqt.polkit-agent-requester-resolver PROPERTIES LABELS "apps;polkit;model;pure"
    )

    qt_add_executable(qindaqt_polkit_dialog_qml_tests tests/tst_polkit_dialog_qml.cpp)
    target_include_directories(
        qindaqt_polkit_dialog_qml_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_compile_definitions(
        qindaqt_polkit_dialog_qml_tests
        PRIVATE
            QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
            QINDAQT_POLKIT_DIALOG_QML_PATH="${CMAKE_CURRENT_SOURCE_DIR}/PolkitDialogContent.qml"
            QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )
    target_link_libraries(
        qindaqt_polkit_dialog_qml_tests
        PRIVATE
            qindaqt_polkit_agent_core
            qindaqt_polkit_agent_appearance
            QindaQt::TokensQml
            qindaqt_themes
            Qt6::Quick
            Qt6::QuickControls2
            Qt6::Test
    )
    add_dependencies(
        qindaqt_polkit_dialog_qml_tests qindaqt_tokens_qmlplugin qindaqt_controls_qmlplugin
    )
    if(TARGET qindaqt_shell_iconsplugin)
        add_dependencies(qindaqt_polkit_dialog_qml_tests qindaqt_shell_iconsplugin)
    endif()
    qindaqt_enable_warnings(qindaqt_polkit_dialog_qml_tests)
    add_test(NAME qindaqt.polkit-agent-dialog-qml COMMAND qindaqt_polkit_dialog_qml_tests)
    set_tests_properties(
        qindaqt.polkit-agent-dialog-qml PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1;QT_FATAL_WARNINGS=1;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
        LABELS "offscreen;apps;polkit;accessibility"
    )
endif()
