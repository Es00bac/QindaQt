# SPDX-License-Identifier: GPL-3.0-or-later

# The QindaLutris library model: pure of UI, with every filesystem, database
# and process reach confined to one small adapter file per source and the
# spawner seam (ADR-0231). Tests link this directly.
find_package(Qt6 REQUIRED COMPONENTS Gui Sql)

qt_add_library(qindaqt_qindalutris_model STATIC
    game.cpp
    game.h
    steam_source.cpp
    steam_source.h
    lutris_source.cpp
    lutris_source.h
    desktop_source.cpp
    desktop_source.h
    wine_source.cpp
    wine_source.h
    game_library.cpp
    game_library.h
    library_store.cpp
    library_store.h
    launch_planner.cpp
    launch_planner.h
    game_launcher.cpp
    game_launcher.h
)
add_library(QindaQt::QindaLutrisModel ALIAS qindaqt_qindalutris_model)

target_include_directories(qindaqt_qindalutris_model
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# AGENT-CONTRACT: the Steam VDF and PE-icon readers are LINKED from the
# compositor's public shell-actions library (ADR-0230), never copied;
# desktop-entry scanning and Exec planning come from the shared apps-side
# application catalog (apps <- shell launcher L0 is its documented direction).
target_link_libraries(qindaqt_qindalutris_model
    PUBLIC
        Qt6::Core
        Qt6::Gui
        QindaQt::ApplicationCatalog
        QindaQt::CompositorShellActions
    PRIVATE
        Qt6::Sql
)
target_compile_features(qindaqt_qindalutris_model PUBLIC cxx_std_20)
set_target_properties(qindaqt_qindalutris_model
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON AUTOUIC OFF)
qindaqt_enable_warnings(qindaqt_qindalutris_model)
