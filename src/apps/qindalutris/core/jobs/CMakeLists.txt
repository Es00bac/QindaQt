# SPDX-License-Identifier: GPL-3.0-or-later

# QindaLutris install jobs (ADR-0275): the download seam and its HTTPS
# allowlist, the GE-Proton release manager (install/remove), the store
# recipes, and the launcher / setup-file install jobs. It deliberately does
# NOT link the library model or the Proton catalog: callers wire the umu
# installer planner and the pinned-build set in through narrow seams
# (installer_planning.h, proton_removal.h). Tests link this directly.
find_package(Qt6 REQUIRED COMPONENTS Core Network)

qt_add_library(qindaqt_qindalutris_jobs STATIC
    job_log.cpp
    job_log.h
    fs_ops.cpp
    fs_ops.h
    download_allowlist.cpp
    download_allowlist.h
    downloader.cpp
    downloader.h
    network_downloader.cpp
    network_downloader.h
    process_runner.cpp
    process_runner.h
    ge_proton_releases.cpp
    ge_proton_releases.h
    proton_install_job.cpp
    proton_install_job.h
    proton_removal.cpp
    proton_removal.h
    prefix_paths.cpp
    prefix_paths.h
    store_recipes.cpp
    store_recipes.h
    install_preflight.cpp
    install_preflight.h
    installer_planning.cpp
    installer_planning.h
    launcher_install_job.cpp
    launcher_install_job.h
    executable_candidates.cpp
    executable_candidates.h
    setup_file_install_job.cpp
    setup_file_install_job.h
)
add_library(QindaQt::QindaLutrisJobs ALIAS qindaqt_qindalutris_jobs)

target_include_directories(qindaqt_qindalutris_jobs
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qindaqt_qindalutris_jobs
    PUBLIC
        Qt6::Core
        Qt6::Network
)
target_compile_features(qindaqt_qindalutris_jobs PUBLIC cxx_std_20)
set_target_properties(qindaqt_qindalutris_jobs
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON AUTOUIC OFF)
qindaqt_enable_warnings(qindaqt_qindalutris_jobs)
