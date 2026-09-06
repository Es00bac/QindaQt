# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt-welcome
    main.cpp
    welcome_actions.cpp
    welcome_actions.h
    welcome_appearance.cpp
    welcome_appearance.h
    welcome_preferences.cpp
    welcome_preferences.h
)
qt_add_qml_module(qindaqt-welcome
    URI QindaQt.WelcomeApp
    VERSION 1.0
    QML_FILES
        Main.qml
        InfoCard.qml
        ShortcutPill.qml
        TutorialDiagram.qml
        TutorialPage.qml
)
target_link_libraries(
    qindaqt-welcome
    PRIVATE
        QindaQt::AppAppearance
        QindaQt::ControlsQml
        QindaQt::DesignTokens
        QindaQt::SettingsClient
        QindaQt::TokensQml
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
add_dependencies(qindaqt-welcome qindaqt_controls_qmlplugin qindaqt_tokens_qmlplugin)
target_compile_definitions(
    qindaqt-welcome
    PRIVATE
        QINDAQT_BUILD_EXECUTABLE_PATH="$<TARGET_FILE:qindaqt-welcome>"
        QINDAQT_BUILD_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
        QINDAQT_INSTALL_QML_RELATIVE_PATH="../${QT6_INSTALL_QML}"
        QINDAQT_INSTALL_WALLPAPER_RELATIVE_PATH="../${CMAKE_INSTALL_DATADIR}/qindaqt/wallpapers"
)
target_compile_features(qindaqt-welcome PRIVATE cxx_std_20)
set_target_properties(
    qindaqt-welcome PROPERTIES
        CXX_EXTENSIONS OFF
        INSTALL_RPATH "$ORIGIN/../${QT6_INSTALL_QML}/QindaQt/Tokens;$ORIGIN/../${QT6_INSTALL_QML}/QindaQt/Controls"
)
qindaqt_enable_warnings(qindaqt-welcome)
if(BUILD_TESTING)
    qt_add_executable(qindaqt_welcome_preferences_tests
        tests/welcome_preferences_test.cpp welcome_preferences.cpp)
    target_link_libraries(qindaqt_welcome_preferences_tests PRIVATE Qt6::Core Qt6::Test)
    target_compile_features(qindaqt_welcome_preferences_tests PRIVATE cxx_std_20)
    qindaqt_enable_warnings(qindaqt_welcome_preferences_tests)
    add_test(NAME qindaqt.welcome-preferences COMMAND qindaqt_welcome_preferences_tests)
    add_test(
        NAME qindaqt.welcome-first-launch-lifecycle
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_welcome_lifecycle.py"
            --welcome "$<TARGET_FILE:qindaqt-welcome>"
            --theme-directory "${PROJECT_SOURCE_DIR}/data/themes"
    )
    set_tests_properties(
        qindaqt.welcome-first-launch-lifecycle
        PROPERTIES
            LABELS "apps;welcome;integration;qml;accessibility"
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1;TMPDIR=${CMAKE_CURRENT_BINARY_DIR}"
            TIMEOUT 30
    )
endif()
install(TARGETS qindaqt-welcome RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Welcome)
install(FILES org.qindaqt.Welcome.desktop DESTINATION "${CMAKE_INSTALL_DATADIR}/applications" COMPONENT Welcome)
# The virtual desktop stage is a standalone package closure used by session tests.
install(TARGETS qindaqt-welcome RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT DesktopVirtual)
install(FILES org.qindaqt.Welcome.desktop DESTINATION "${CMAKE_INSTALL_DATADIR}/applications" COMPONENT DesktopVirtual)
