# SPDX-License-Identifier: GPL-3.0-or-later
# qindaqt-osk: the first-party on-screen keyboard the compositor launches as
# its input method (ADR-0204). The panel role needs Qt's Wayland client
# private API, so the app is skipped, with a notice, where that is absent.

find_package(Qt6 6.11 QUIET COMPONENTS DBus WaylandClient WaylandClientPrivate)
find_package(WaylandProtocols 1.31 QUIET)

set(_qindaqt_osk_missing "")
if(NOT TARGET Qt6::WaylandClientPrivate)
    list(APPEND _qindaqt_osk_missing "Qt6::WaylandClientPrivate")
endif()
if(NOT COMMAND qt6_generate_wayland_protocol_client_sources)
    list(APPEND _qindaqt_osk_missing "Qt6WaylandScannerTools")
endif()
if(NOT WaylandProtocols_FOUND)
    list(APPEND _qindaqt_osk_missing "wayland-protocols")
endif()
find_path(QINDAQT_XKBCOMMON_KEYSYMS_DIR xkbcommon/xkbcommon-keysyms.h)
if(NOT QINDAQT_XKBCOMMON_KEYSYMS_DIR)
    list(APPEND _qindaqt_osk_missing "xkbcommon-keysyms.h")
endif()
if(_qindaqt_osk_missing)
    message(STATUS "qindaqt-osk: not built (missing: ${_qindaqt_osk_missing})")
    return()
endif()

# The wayland-scanner glue for the protocol is C.
enable_language(C)

# The keyboard's state machine, layouts and layout source: no Wayland, so the
# unit rows exercise exactly what the panel paints and emits.
qt_add_library(qindaqt_osk_core STATIC)
target_sources(
    qindaqt_osk_core
    PRIVATE
        osk_evidence.cpp
        osk_evidence.h
        osk_keyboard_model.cpp
        osk_keyboard_model.h
        osk_layout_catalog.cpp
        osk_layout_catalog.h
        osk_layout_document.cpp
        osk_layout_document.h
        osk_layout_source.cpp
        osk_layout_source.h
)
qt_add_resources(
    qindaqt_osk_core "qindaqt_osk_layouts"
    PREFIX "/qt/qml/QindaQt/OskApp/layouts"
    BASE layouts
    FILES
        layouts/de.json
        layouts/es.json
        layouts/fr.json
        layouts/gb.json
        layouts/us.json
)
target_include_directories(qindaqt_osk_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${QINDAQT_XKBCOMMON_KEYSYMS_DIR}")
target_link_libraries(qindaqt_osk_core PUBLIC Qt6::Core Qt6::DBus Qt6::Gui)
target_compile_features(qindaqt_osk_core PUBLIC cxx_std_20)
set_target_properties(qindaqt_osk_core PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
qindaqt_enable_warnings(qindaqt_osk_core)

qt_add_executable(
    qindaqt-osk
    input_method_client.cpp
    input_method_client.h
    input_panel_integration.cpp
    input_panel_integration.h
    main.cpp
    osk_appearance.cpp
    osk_appearance.h
)
# The generated protocol glue is third-party shaped code: it lives in its own
# target, outside the project's strict warnings.
qt_add_library(qindaqt_osk_protocol STATIC)
qt6_generate_wayland_protocol_client_sources(
    qindaqt_osk_protocol
    PRIVATE_CODE
    NO_INCLUDE_CORE_ONLY
    FILES "${WaylandProtocols_DATADIR}/unstable/input-method/input-method-unstable-v1.xml"
)
target_include_directories(qindaqt_osk_protocol PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(qindaqt_osk_protocol PUBLIC Qt6::WaylandClient)
target_compile_options(qindaqt_osk_protocol PRIVATE -w)
set_target_properties(qindaqt_osk_protocol PROPERTIES POSITION_INDEPENDENT_CODE ON)
qt_add_qml_module(
    qindaqt-osk
    URI QindaQt.OskApp
    VERSION 1.0
    QML_FILES
        qml/KeyCap.qml
        qml/OnScreenKeyboard.qml
)
target_link_libraries(
    qindaqt-osk
    PRIVATE
        qindaqt_osk_core
        qindaqt_osk_protocol
        QindaQt::AppAppearance
        QindaQt::DesignTokens
        QindaQt::SettingsClient
        QindaQt::TokensQml
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::WaylandClient
        Qt6::WaylandClientPrivate
)
add_dependencies(qindaqt-osk qindaqt_tokens_qmlplugin)
target_compile_definitions(
    qindaqt-osk
    PRIVATE
        QINDAQT_BUILD_EXECUTABLE_PATH="$<TARGET_FILE:qindaqt-osk>"
        QINDAQT_BUILD_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
        QINDAQT_INSTALL_QML_RELATIVE_PATH="../${QT6_INSTALL_QML}"
)
target_compile_features(qindaqt-osk PRIVATE cxx_std_20)
set_target_properties(
    qindaqt-osk PROPERTIES
        CXX_EXTENSIONS OFF
        INSTALL_RPATH "$ORIGIN/../${QT6_INSTALL_QML}/QindaQt/Tokens"
)
qindaqt_enable_warnings(qindaqt-osk)

if(BUILD_TESTING)
    qt_add_executable(qindaqt_osk_keyboard_model_tests tests/tst_osk_keyboard_model.cpp)
    target_link_libraries(qindaqt_osk_keyboard_model_tests PRIVATE qindaqt_osk_core Qt6::Test)
    target_compile_features(qindaqt_osk_keyboard_model_tests PRIVATE cxx_std_20)
    qindaqt_enable_warnings(qindaqt_osk_keyboard_model_tests)
    add_test(NAME qindaqt.osk-keyboard-model COMMAND qindaqt_osk_keyboard_model_tests)
    set_tests_properties(qindaqt.osk-keyboard-model PROPERTIES LABELS "apps;osk;touch")
endif()

install(TARGETS qindaqt-osk RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT OnScreenKeyboard)
install(
    FILES org.qindaqt.OnScreenKeyboard.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
    COMPONENT OnScreenKeyboard
)
install(TARGETS qindaqt-osk RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT DesktopVirtual)
install(
    FILES org.qindaqt.OnScreenKeyboard.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
    COMPONENT DesktopVirtual
)
