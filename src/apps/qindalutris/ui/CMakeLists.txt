# SPDX-License-Identifier: GPL-3.0-or-later

# The QML-facing view layer: list/filter models, the composition controller,
# and the cover image provider. A library (not part of the executable) so
# tests link it without dragging in a window.
qt_add_library(qindaqt_qindalutris_view STATIC
    game_icon_provider.h
    game_list_model.cpp
    game_list_model.h
    library_controller.cpp
    library_controller.h
    qindalutris_actions.cpp
    qindalutris_actions.h
)
add_library(QindaQt::QindaLutrisView ALIAS qindaqt_qindalutris_view)

target_include_directories(qindaqt_qindalutris_view
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qindaqt_qindalutris_view
    PUBLIC QindaQt::QindaLutrisModel QindaQt::AppShell Qt6::Core Qt6::Gui
    PRIVATE Qt6::Qml Qt6::Quick
)
target_compile_features(qindaqt_qindalutris_view PUBLIC cxx_std_20)
set_target_properties(qindaqt_qindalutris_view
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON AUTOUIC OFF)
qindaqt_enable_warnings(qindaqt_qindalutris_view)
