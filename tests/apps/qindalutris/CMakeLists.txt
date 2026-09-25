# SPDX-License-Identifier: GPL-3.0-or-later

# Focused rows for QindaLutris (ADR-0231, ADR-0275): fixture-backed model rows, the
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
# ADR-0275: pinned Proton builds, installed titles, umu launch plans.
qindaqt_add_qindalutris_test(proton-catalog tst_proton_catalog.cpp)
qindaqt_add_qindalutris_test(proton-pin tst_proton_pin.cpp)
qindaqt_add_qindalutris_test(title-store tst_title_store.cpp)
qindaqt_add_qindalutris_test(umu-launch-plan tst_umu_launch_plan.cpp)

qindaqt_add_qindalutris_test(lutris-source tst_lutris_source.cpp)
# The Lutris fixture builds its own database, so this row links Qt Sql too.
target_link_libraries(qindaqt_qindalutris_lutris-source_tests PRIVATE Qt6::Sql)

# Install jobs and store recipes (ADR-0275): scripted downloader, runner and
# host-probe fakes (job_fakes.h); the Proton rows also run the host's real
# `tar` on archives they build themselves, the process rows run sh/sleep
# trees, and the network rows talk to a local 127.0.0.1 server. No row
# reaches the internet, umu, Wine or a vendor installer. The one systemd
# scope row uses the real user manager when it answers and skips otherwise.
function(qindaqt_add_qindalutris_jobs_test name source)
    qt_add_executable(qindaqt_qindalutris_${name}_tests ${source} job_fakes.h proton_job_fixture.h)
    target_link_libraries(qindaqt_qindalutris_${name}_tests
        PRIVATE QindaQt::QindaLutrisJobs Qt6::Test)
    target_compile_features(qindaqt_qindalutris_${name}_tests PRIVATE cxx_std_20)
    set_target_properties(qindaqt_qindalutris_${name}_tests
        PROPERTIES CXX_EXTENSIONS OFF AUTOUIC OFF)
    qindaqt_enable_warnings(qindaqt_qindalutris_${name}_tests)
    add_test(NAME qindaqt.qindalutris-${name} COMMAND qindaqt_qindalutris_${name}_tests)
    set_tests_properties(qindaqt.qindalutris-${name}
        PROPERTIES LABELS "apps;qindalutris;jobs" TIMEOUT 60
        ENVIRONMENT "${qindalutris_test_env}")
endfunction()

qindaqt_add_qindalutris_jobs_test(download-allowlist tst_download_allowlist.cpp)
qindaqt_add_qindalutris_jobs_test(download-guard tst_download_guard.cpp)
qindaqt_add_qindalutris_jobs_test(network-downloader tst_network_downloader.cpp)
qindaqt_add_qindalutris_jobs_test(ge-proton-releases tst_ge_proton_releases.cpp)
qindaqt_add_qindalutris_jobs_test(archive-listing tst_archive_listing.cpp)
qindaqt_add_qindalutris_jobs_test(staged-tree-check tst_staged_tree_check.cpp)
qindaqt_add_qindalutris_jobs_test(proton-install-job tst_proton_install_job.cpp)
qindaqt_add_qindalutris_jobs_test(proton-install-cancel tst_proton_install_cancel.cpp)
qindaqt_add_qindalutris_jobs_test(proton-removal tst_proton_removal.cpp)
qindaqt_add_qindalutris_jobs_test(process-runner tst_process_runner.cpp)
qindaqt_add_qindalutris_jobs_test(store-recipes tst_store_recipes.cpp)
qindaqt_add_qindalutris_jobs_test(install-preflight tst_install_preflight.cpp)
qindaqt_add_qindalutris_jobs_test(launcher-install-job tst_launcher_install_job.cpp)
qindaqt_add_qindalutris_jobs_test(setup-file-install-job tst_setup_file_install_job.cpp)

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
