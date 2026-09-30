# SPDX-License-Identifier: GPL-3.0-or-later
add_library(qindaqt_lock_worker_client STATIC worker_process.cpp worker_process.h)
target_include_directories(qindaqt_lock_worker_client PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qindaqt_lock_worker_client PUBLIC qindaqt_lock_worker_wire qindaqt_lock_platform Qt6::Core)
target_compile_definitions(qindaqt_lock_worker_client PRIVATE QINDAQT_LOCK_PAM_EXECUTABLE="${KDE_INSTALL_FULL_LIBEXECDIR}/qindaqt-lock-pam")
qindaqt_enable_warnings(qindaqt_lock_worker_client)

if(BUILD_TESTING)
    qt_add_executable(tst_lock_worker_process tests/tst_worker_process.cpp worker_process.cpp worker_process.h)
    target_include_directories(tst_lock_worker_process PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(tst_lock_worker_process PRIVATE QINDAQT_PRIVATE_WORKER_CLIENT_FIXTURE
        QINDAQT_LOCK_PAM_EXECUTABLE="${KDE_INSTALL_FULL_LIBEXECDIR}/qindaqt-lock-pam"
        QINDAQT_PRIVATE_WORKER_PATH="$<TARGET_FILE:qindaqt-private-lock-pam>"
        QINDAQT_PRIVATE_PAM_MODULE_PATH="$<TARGET_FILE:qindaqt-private-pam-witness>")
    target_link_libraries(tst_lock_worker_process PRIVATE qindaqt_lock_worker_wire qindaqt_lock_platform Qt6::Test)
    add_dependencies(tst_lock_worker_process qindaqt-private-lock-pam qindaqt-private-pam-witness)
    qindaqt_enable_warnings(tst_lock_worker_process)
    add_test(NAME lock.worker-process COMMAND tst_lock_worker_process)
endif()
