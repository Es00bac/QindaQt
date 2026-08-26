# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt-shell-preview
    app/captureenvironment.cpp
    app/captureenvironment.h
    app/main.cpp
    app/previewoptions.cpp
    app/previewoptions.h
    app/screenshotcapture.cpp
    app/screenshotcapture.h
    app/shellpreviewapplication.cpp
    app/shellpreviewapplication.h
)

qt_add_qml_module(
    qindaqt-shell-preview
    URI QindaQt.Shell
    VERSION 1.0
    QML_FILES
        qml/AppletChip.qml
        qml/ContainerTabStrip.qml
        qml/DecorationButtons.qml
        qml/DesktopPreview.qml
        qml/Main.qml
        qml/MemberWindow.qml
        qml/PanelAppletRow.qml
        qml/PanelSurface.qml
        qml/PreviewControls.qml
        qml/WindowGroupPreview.qml
)

target_link_libraries(
    qindaqt-shell-preview
    PRIVATE
        qindaqt_profiles
        qindaqt_themes
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)

target_compile_definitions(
    qindaqt-shell-preview
    PRIVATE
        QINDAQT_VERSION="${PROJECT_VERSION}"
        QINDAQT_SOURCE_PROFILE_DIR="${PROJECT_SOURCE_DIR}/data/profiles"
        QINDAQT_SOURCE_THEME_DIR="${PROJECT_SOURCE_DIR}/data/themes"
)

qindaqt_enable_warnings(qindaqt-shell-preview)

install(TARGETS qindaqt-shell-preview RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
