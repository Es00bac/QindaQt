# SPDX-License-Identifier: GPL-3.0-or-later

# Appearance domain of the first-party settings application: validated
# appearance values, pure QST-1 preview projection, and the Settings1-backed
# route model. Consumes only public service client, themes, and design-token
# boundaries; the qindaqt-settings executable owns the route seam.
qt_add_library(qindaqt_settings_appearance STATIC)
target_sources(
    qindaqt_settings_appearance
    PRIVATE
        appearance_values.cpp
        appearance_preview.cpp
        appearance_qml_composition.cpp
        appearance_settings_model.cpp
        appearance_settings_model_draft.cpp
        appearance_settings_model_preview.cpp
        appearance_settings_model_results.cpp
        appearance_theme_catalog.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/apps/settings_appearance/appearance_values.h
            include/qindaqt/apps/settings_appearance/appearance_preview.h
            include/qindaqt/apps/settings_appearance/appearance_qml_composition.h
            include/qindaqt/apps/settings_appearance/appearance_settings_model.h
            include/qindaqt/apps/settings_appearance/appearance_theme_catalog.h
)
target_link_libraries(
    qindaqt_settings_appearance
    # TokenFacade's implementation lives in the shared QindaQt.Tokens QML
    # module library; the static DesignTokens value library does not carry it.
    PUBLIC QindaQt::SettingsClient QindaQt::DesignTokens QindaQt::TokensQml
           qindaqt_themes Qt6::Core Qt6::Gui Qt6::Qml
)
target_compile_features(qindaqt_settings_appearance PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_settings_appearance
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_settings_appearance)

set(QINDAQT_APPEARANCE_QML_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/qml/QindaQt/SettingsApp/Appearance")
qt_add_library(qindaqt_settings_appearance_qml SHARED)
qt_add_qml_module(
    qindaqt_settings_appearance_qml
    URI QindaQt.SettingsApp.Appearance
    VERSION 1.0
    CLASS_NAME QindaQtSettingsAppearancePlugin
    OUTPUT_DIRECTORY "${QINDAQT_APPEARANCE_QML_OUTPUT_DIRECTORY}"
    TYPEINFO qindaqt_settings_appearance.qmltypes
    QML_FILES
        qml/AppearanceDesktopSection.qml
        qml/AppearanceFontSection.qml
        qml/AppearancePage.qml
        qml/AppearanceThemeSection.qml
        qml/SegmentedChoiceRow.qml
)
target_link_libraries(
    qindaqt_settings_appearance_qml
    PRIVATE QindaQt::TokensQml Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
            Qt6::QuickControls2
)
set_target_properties(
    qindaqt_settings_appearance_qml
    PROPERTIES
        CXX_EXTENSIONS OFF
        # AGENT-CONTRACT: Sibling QML modules (Tokens, Controls) resolve from
        # the installed QML tree; keep the relative rpath so a staged or
        # relocated prefix stays runnable.
        INSTALL_RPATH "$ORIGIN/../../Tokens;$ORIGIN/../../Controls"
)
qindaqt_enable_warnings(qindaqt_settings_appearance_qml)

# AGENT-NOTE: The backing library and generated plugin must remain beside
# qmldir in an installed prefix; see the Tokens module for the same contract.
install(
    TARGETS qindaqt_settings_appearance_qml qindaqt_settings_appearance_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Appearance"
        COMPONENT SettingsAppearanceRuntime
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Appearance"
        COMPONENT SettingsAppearanceRuntime
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Appearance"
        COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        "${QINDAQT_APPEARANCE_QML_OUTPUT_DIRECTORY}/qmldir"
        "${QINDAQT_APPEARANCE_QML_OUTPUT_DIRECTORY}/qindaqt_settings_appearance.qmltypes"
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Appearance"
    COMPONENT SettingsAppearanceRuntime
)
install(
    FILES
        qml/AppearanceDesktopSection.qml
        qml/AppearanceFontSection.qml
        qml/AppearancePage.qml
        qml/AppearanceThemeSection.qml
        qml/SegmentedChoiceRow.qml
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/SettingsApp/Appearance"
    COMPONENT SettingsAppearanceRuntime
)
