# SPDX-License-Identifier: GPL-3.0-or-later

# Focused rows for QindaLutris (ADR-0231): fixture-backed model rows, the
# store contract, the launch planner, and the QML page over a stub model.
# Everything runs against synthetic fixtures in a private HOME -- never the
# operator's real Steam, Lutris, or desktop data.

find_package(Qt6 REQUIRED COMPONENTS QuickTest Sql)

set(qindalutris_test_root "${CMAKE_CURRENT_BINARY_DIR}/isolated")
file(MAKE_DIRECTORY "${qindalutris_test_root}/home" "${qindalutris_test_root}/runtime")
file(CHMOD "${qindalutris_test_root}/runtime" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
set(qindalutris_test_env
    "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software;QT_QUICK_BACKEND=software;QT_QPA_PLATFORMTHEME=;HOME=${qindalutris_test_root}/home;XDG_CONFIG_HOME=${qindalutris_test_root}/home/config;XDG_DATA_HOME=${qindalutris_test_root}/home/data;XDG_CACHE_HOME=${qindalutris_test_root}/home/cache;XDG_RUNTIME_DIR=${qindalutris_test_root}/runtime;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent;DISPLAY=;WAYLAND_DISPLAY=")

function(qindaqt_add_qindalutris_test name source)
    qt_add_executable(qindaqt_qindalutris_${name}_tests ${source})
    target_link_libraries(qindaqt_qindalutris_${name}_tests
        PRIVATE QindaQt::QindaLutrisModel Qt6::Test)
    target_compile_features(qindaqt_qindalutris_${name}_tests PRIVATE cxx_std_20)
    set_target_properties(qindaqt_qindalutris_${name}_tests
        PROPERTIES CXX_EXTENSIONS OFF AUTOUIC OFF)
    qindaqt_enable_warnings(qindaqt_qindalutris_${name}_tests)
    add_test(NAME qindaqt.qindalutris-${name} COMMAND qindaqt_qindalutris_${name}_tests)
    set_tests_properties(qindaqt.qindalutris-${name}
        PROPERTIES LABELS "apps;qindalutris" TIMEOUT 30
        ENVIRONMENT "${qindalutris_test_env}")
endfunction()

qindaqt_add_qindalutris_test(steam-source tst_steam_source.cpp)
qindaqt_add_qindalutris_test(desktop-source tst_desktop_source.cpp)
qindaqt_add_qindalutris_test(game-library tst_game_library.cpp)
qindaqt_add_qindalutris_test(library-store tst_library_store.cpp)
qindaqt_add_qindalutris_test(launch-planner tst_launch_planner.cpp)
qindaqt_add_qindalutris_test(wine-source tst_wine_source.cpp)

# The compatibility database (ADR-0275 §3). The fixtures under data/compat are
# shared with the generator's Python tests, and the committed snapshot is
# loaded by both suites so the C++ parser and qlcompat/schema.py cannot drift.
qindaqt_add_qindalutris_test(compat-db tst_compat_db.cpp)
qindaqt_add_qindalutris_test(compat-lookup tst_compat_lookup.cpp)
foreach(qindalutris_compat_test compat-db compat-lookup)
    target_compile_definitions(qindaqt_qindalutris_${qindalutris_compat_test}_tests PRIVATE
        "QINDALUTRIS_COMPAT_FIXTURES=\"${CMAKE_CURRENT_SOURCE_DIR}/data/compat\""
        "QINDALUTRIS_COMPAT_SNAPSHOT=\"${PROJECT_SOURCE_DIR}/src/apps/qindalutris/compat/compat-db-v1.json\"")
endforeach()
set_tests_properties(qindaqt.qindalutris-compat-db PROPERTIES TIMEOUT 90)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(
    NAME qindaqt.qindalutris-compat-generator
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${PROJECT_SOURCE_DIR}/tools/qindalutris-compat/tests"
        -t "${PROJECT_SOURCE_DIR}/tools/qindalutris-compat"
)
add_test(
    NAME qindaqt.qindalutris-compat-snapshot
    COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tools/qindalutris-compat/validate.py"
        "${PROJECT_SOURCE_DIR}/src/apps/qindalutris/compat/compat-db-v1.json"
)
set_tests_properties(qindaqt.qindalutris-compat-generator qindaqt.qindalutris-compat-snapshot
    PROPERTIES LABELS "apps;qindalutris;tools" TIMEOUT 120
    ENVIRONMENT "${qindalutris_test_env}")

qindaqt_add_qindalutris_test(lutris-source tst_lutris_source.cpp)
# The Lutris fixture builds its own database, so this row links Qt Sql too.
target_link_libraries(qindaqt_qindalutris_lutris-source_tests PRIVATE Qt6::Sql)

# The composition root: injected roots, a recording spawner, and the real
# store. Runs offscreen (QScreen enumeration is part of the contract).
qt_add_executable(qindaqt_qindalutris_controller_tests tst_library_controller.cpp)
target_link_libraries(qindaqt_qindalutris_controller_tests
    PRIVATE QindaQt::QindaLutrisView Qt6::Gui Qt6::Test)
target_compile_features(qindaqt_qindalutris_controller_tests PRIVATE cxx_std_20)
set_target_properties(qindaqt_qindalutris_controller_tests
    PROPERTIES CXX_EXTENSIONS OFF AUTOUIC OFF)
qindaqt_enable_warnings(qindaqt_qindalutris_controller_tests)
add_test(NAME qindaqt.qindalutris-controller
    COMMAND qindaqt_qindalutris_controller_tests)
set_tests_properties(qindaqt.qindalutris-controller
    PROPERTIES LABELS "apps;qindalutris" TIMEOUT 45
    ENVIRONMENT "${qindalutris_test_env}")

# The page test runs the stock qmltestrunner against the built module.
add_test(
    NAME qindaqt.qindalutris-page-offscreen
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_library_page.qml"
)
set_tests_properties(qindaqt.qindalutris-page-offscreen
    PROPERTIES LABELS "apps;qindalutris;qml;offscreen" TIMEOUT 60
    ENVIRONMENT "${qindalutris_test_env}")

# The --grab verification path: the real application renders one frame
# offscreen into the artifacts directory, proving the window constructs
# end-to-end. HOME is the isolated fixture root, so no real machine data
# participates.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/artifacts")
add_test(
    NAME qindaqt.qindalutris-grab-offscreen
    COMMAND $<TARGET_FILE:qindalutris>
        --grab "${CMAKE_CURRENT_BINARY_DIR}/artifacts/qindalutris.png"
        --grab-after 2500 --size 1180x760
)
set_tests_properties(qindaqt.qindalutris-grab-offscreen
    PROPERTIES LABELS "apps;qindalutris;grab;offscreen" TIMEOUT 90
    ENVIRONMENT "${qindalutris_test_env}")
