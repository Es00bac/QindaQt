# SPDX-License-Identifier: GPL-3.0-or-later

# The one renderer for QindaQt window chrome (ADR-0127). Linked into the
# KDecoration plugin (live windows) and the Settings Appearance preview; it
# depends on Qt Gui, the theme spec, and the presentation-neutral chrome
# palette types only, never on KDecoration or KWin.
find_package(Qt6 6.11 REQUIRED COMPONENTS Gui)

qt_add_library(qindaqt_decoration_painter STATIC)
add_library(QindaQt::DecorationPainter ALIAS qindaqt_decoration_painter)
target_sources(
    qindaqt_decoration_painter
    PRIVATE
        src/decoration_painter.cpp
        src/decoration_buttons.cpp
        src/chrome_preferences.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES include/qindaqt/decoration_painter/decoration_painter.h
)
target_link_libraries(
    qindaqt_decoration_painter
    PUBLIC
        QindaQt::HybridChrome
        qindaqt_themes
        Qt6::Gui
)
target_compile_features(qindaqt_decoration_painter PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_decoration_painter
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_decoration_painter)
