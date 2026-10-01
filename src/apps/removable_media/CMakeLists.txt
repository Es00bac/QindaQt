# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_library(qindaqt_removable_media_core STATIC)
target_sources(qindaqt_removable_media_core PRIVATE
    media_backend.h media_types.cpp media_types.h
    media_preferences.cpp media_preferences.h
    media_controller.cpp media_controller.h
    udisks_backend.cpp udisks_backend.h udisks_operations.cpp
    media_notifications.cpp media_notifications.h)
target_include_directories(qindaqt_removable_media_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qindaqt_removable_media_core PUBLIC Qt6::Core Qt6::DBus)
target_compile_features(qindaqt_removable_media_core PUBLIC cxx_std_20)
set_target_properties(qindaqt_removable_media_core PROPERTIES
    CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
qindaqt_enable_warnings(qindaqt_removable_media_core)

qt_add_library(qindaqt_removable_media_appearance STATIC
    media_appearance.cpp media_appearance.h)
target_include_directories(qindaqt_removable_media_appearance PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qindaqt_removable_media_appearance PUBLIC
    QindaQt::AppAppearance QindaQt::DesignTokens Qt6::Core Qt6::Gui Qt6::Qml)
target_compile_features(qindaqt_removable_media_appearance PUBLIC cxx_std_20)
qindaqt_enable_warnings(qindaqt_removable_media_appearance)

qt_add_executable(qindaqt-removable-media main.cpp)
qt_add_qml_module(qindaqt-removable-media
    URI QindaQt.RemovableMediaApp VERSION 1.0
    QML_FILES Main.qml MediaDetails.qml FormatDialog.qml)
target_link_libraries(qindaqt-removable-media PRIVATE
    qindaqt_removable_media_core qindaqt_removable_media_appearance
    QindaQt::ControlsQml QindaQt::TokensQml QindaQt::SettingsClient
    Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
target_compile_features(qindaqt-removable-media PRIVATE cxx_std_20)
target_compile_definitions(qindaqt-removable-media PRIVATE
    QINDAQT_BUILD_EXECUTABLE_PATH="$<TARGET_FILE:qindaqt-removable-media>"
    QINDAQT_BUILD_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_INSTALL_QML_RELATIVE_PATH="../${QT6_INSTALL_QML}")
set_target_properties(qindaqt-removable-media PROPERTIES
    CXX_EXTENSIONS OFF
    INSTALL_RPATH "$ORIGIN/../${QT6_INSTALL_QML}/QindaQt/Tokens;$ORIGIN/../${QT6_INSTALL_QML}/QindaQt/Controls")
add_dependencies(qindaqt-removable-media qindaqt_controls_qmlplugin qindaqt_tokens_qmlplugin)
qindaqt_enable_warnings(qindaqt-removable-media)

if(BUILD_TESTING)
    find_program(QINDAQT_MEDIA_DBUS_RUN_SESSION dbus-run-session REQUIRED)
    foreach(behavior policy udisks notifications)
        qt_add_executable(qindaqt_media_${behavior}_tests tests/tst_media_${behavior}.cpp)
        target_link_libraries(qindaqt_media_${behavior}_tests PRIVATE
            qindaqt_removable_media_core Qt6::Test)
        qindaqt_enable_warnings(qindaqt_media_${behavior}_tests)
        if(NOT behavior STREQUAL "policy")
            add_test(NAME qindaqt.removable-media-${behavior}
                COMMAND "${QINDAQT_MEDIA_DBUS_RUN_SESSION}" -- $<TARGET_FILE:qindaqt_media_${behavior}_tests>)
            set_tests_properties(qindaqt.removable-media-${behavior} PROPERTIES
                ENVIRONMENT "DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
                LABELS "apps;media;integration;private-bus" TIMEOUT 60)
        else()
            add_test(NAME qindaqt.removable-media-policy COMMAND qindaqt_media_policy_tests)
            set_tests_properties(qindaqt.removable-media-policy PROPERTIES LABELS "apps;media;policy;pure")
        endif()
    endforeach()
    qt_add_executable(qindaqt_media_qml_tests tests/tst_media_qml.cpp)
    target_link_libraries(qindaqt_media_qml_tests PRIVATE
        qindaqt_removable_media_core qindaqt_removable_media_appearance
        QindaQt::ControlsQml QindaQt::TokensQml qindaqt_themes
        Qt6::Quick Qt6::QuickControls2 Qt6::Test)
    target_compile_definitions(qindaqt_media_qml_tests PRIVATE
        QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
        QINDAQT_MEDIA_QML_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Main.qml"
        QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_dependencies(qindaqt_media_qml_tests qindaqt_controls_qmlplugin qindaqt_tokens_qmlplugin)
    qindaqt_enable_warnings(qindaqt_media_qml_tests)
    add_test(NAME qindaqt.removable-media-qml COMMAND qindaqt_media_qml_tests)
    set_tests_properties(qindaqt.removable-media-qml PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
        LABELS "apps;media;qml;offscreen")
endif()

install(TARGETS qindaqt-removable-media RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT RemovableMedia)
install(FILES org.qindaqt.RemovableMedia.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications" COMPONENT RemovableMedia)
install(TARGETS qindaqt-removable-media RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT DesktopVirtual)
install(FILES org.qindaqt.RemovableMedia.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications" COMPONENT DesktopVirtual)
