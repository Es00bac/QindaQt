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
    # ADR-0275: pinned Proton builds, installed titles, umu launches.
    store_io.cpp
    store_io.h
    proton_catalog.cpp
    proton_catalog.h
    proton_pin.cpp
    proton_pin.h
    title_record.cpp
    title_record.h
    title_store.cpp
    title_store.h
    title_source.cpp
    title_source.h
    store_launch.cpp
    store_launch.h
    umu_launch.cpp
    umu_launch.h
    preferences_store.cpp
    preferences_store.h
    wine_pin_migration.cpp
    wine_pin_migration.h
    compat_db.cpp
    compat_db.h
    compat_db_parse.cpp
    compat_db_rules.cpp
    compat_db_rules.h
    compat_json_scan.cpp
    compat_json_scan.h
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

# Install jobs and store recipes (ADR-0275); a sibling library, not a
# dependency of the model.
add_subdirectory(jobs)

# AGENT-CONTRACT: the shipped compatibility database path is derived from the
# same install datadir that src/apps/qindalutris/CMakeLists.txt installs
# compat/compat-db-v1.json into (ADR-0275 §3), so reader and installer agree.
set(qindalutris_compat_datadir "${CMAKE_INSTALL_DATADIR}")
cmake_path(ABSOLUTE_PATH qindalutris_compat_datadir
    BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}" NORMALIZE)
target_compile_definitions(qindaqt_qindalutris_model PRIVATE
    "QINDALUTRIS_COMPAT_DB_SHIPPED_PATH=\"${qindalutris_compat_datadir}/qindalutris/compat-db-v1.json\"")

# AGENT-CONTRACT: tools/qindalutris-compat/winetricks-verbs.txt is the single
# winetricks verb allowlist for BOTH compat-db-v1 validators. The Python side
# reads it directly; here it becomes a generated include for
# compat_db_rules.cpp. Editing the list re-runs configure.
set(qindalutris_verb_list "${PROJECT_SOURCE_DIR}/tools/qindalutris-compat/winetricks-verbs.txt")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${qindalutris_verb_list}")
file(STRINGS "${qindalutris_verb_list}" qindalutris_verb_lines)
set(qindalutris_verb_entries "")
foreach(qindalutris_verb IN LISTS qindalutris_verb_lines)
    string(STRIP "${qindalutris_verb}" qindalutris_verb)
    if(qindalutris_verb STREQUAL "" OR qindalutris_verb MATCHES "^#")
        continue()
    endif()
    if(NOT qindalutris_verb MATCHES "^[a-z0-9_=.-]+$")
        message(FATAL_ERROR "${qindalutris_verb_list}: bad verb line '${qindalutris_verb}'")
    endif()
    string(APPEND qindalutris_verb_entries "    \"${qindalutris_verb}\",\n")
endforeach()
set(qindalutris_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(CONFIGURE OUTPUT "${qindalutris_generated_dir}/compat_winetricks_verbs.inc"
    CONTENT "// GENERATED from tools/qindalutris-compat/winetricks-verbs.txt by\n// src/apps/qindalutris/core/CMakeLists.txt. Do not edit.\n${qindalutris_verb_entries}" @ONLY)
target_include_directories(qindaqt_qindalutris_model PRIVATE "${qindalutris_generated_dir}")
