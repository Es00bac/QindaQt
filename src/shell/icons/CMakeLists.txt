# SPDX-License-Identifier: LGPL-3.0-or-later

# Shell iconography I1: confined XDG icon-theme resolution (IconThemeLocator),
# desktop-entry icon resolution (DesktopEntryIconResolver), the
# image://qindaqt-icon/ provider plus the IconRuntime composition seam, and
# the compiled QindaQt.Shell.Icons module carrying the Icon element. STATIC
# target (task-list applet precedent): composed in-process by the audited
# shell, never loaded as third-party code.

find_package(Qt6 6.11 REQUIRED COMPONENTS Gui Qml Quick Svg)

qt_add_library(qindaqt_shell_icons STATIC)
add_library(QindaQt::ShellIcons ALIAS qindaqt_shell_icons)

target_sources(
    qindaqt_shell_icons
    PRIVATE
        src/desktop_entry_icon_resolver.cpp
        src/icon_image_provider.cpp
        src/icon_lookup.cpp
        src/icon_runtime.cpp
        src/icon_theme_locator.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/icons/desktop_entry_icon_resolver.h
            include/qindaqt/shell/icons/icon_image_provider.h
            include/qindaqt/shell/icons/icon_lookup.h
            include/qindaqt/shell/icons/icon_runtime.h
            include/qindaqt/shell/icons/icon_theme_limits.h
            include/qindaqt/shell/icons/icon_theme_locator.h
)

qt_add_qml_module(
    qindaqt_shell_icons
    URI QindaQt.Shell.Icons
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/Icons"
    TYPEINFO qindaqt_shell_icons.qmltypes
    SOURCES
        include/qindaqt/shell/icons/icon_lookup.h
        src/icon_lookup.cpp
    QML_FILES
        qml/Icon.qml
)

# AGENT-CONTRACT: The module consumes the launcher's public pure
# desktop-entry parser and Qt Core/Gui/Qml/Quick/Svg only. It must never link
# D-Bus, KWin, LayerShellQt, the shell runtime, or the status-notifier
# modules, and never touches the network or writes to the filesystem; the
# boundary test in tests/shell/icons enforces this.
target_link_libraries(
    qindaqt_shell_icons
    PUBLIC
        QindaQt::ShellLauncher
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Svg
)
target_compile_features(qindaqt_shell_icons PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_icons PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_icons)
# The generated QML type-registration unit includes the QML_ELEMENT header as
# <icon_lookup.h> (design-tokens precedent); expose its directory privately.
target_include_directories(
    qindaqt_shell_icons
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/qindaqt/shell/icons"
)

install(
    TARGETS qindaqt_shell_icons
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# The compiled module imports QindaQt.Tokens; that module must be built
# before qmlcachegen runs. In the repository build this directory is
# processed before src/design_tokens, so the ordering hook defers to the end
# of the src scope (task-list applet precedent).
function(qindaqt_shell_icons_qml_dependencies)
    if(TARGET qindaqt_shell_icons AND TARGET qindaqt_tokens_qmlplugin)
        add_dependencies(qindaqt_shell_icons qindaqt_tokens_qmlplugin)
    endif()
endfunction()
if(TARGET qindaqt_tokens_qmlplugin)
    qindaqt_shell_icons_qml_dependencies()
else()
    get_filename_component(QINDAQT_SHELL_ICONS_REPO_ROOT
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
    cmake_language(
        DEFER DIRECTORY "${QINDAQT_SHELL_ICONS_REPO_ROOT}/src"
        CALL qindaqt_shell_icons_qml_dependencies
    )
endif()
