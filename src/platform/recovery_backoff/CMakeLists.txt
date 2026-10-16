# SPDX-License-Identifier: LGPL-3.0-or-later
qt_add_library(qindaqt_recovery_backoff STATIC)
add_library(QindaQt::RecoveryBackoff ALIAS qindaqt_recovery_backoff)
target_sources(qindaqt_recovery_backoff PRIVATE src/recovery_backoff.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
    include/qindaqt/platform/recovery_backoff/recovery_backoff.h)
target_link_libraries(qindaqt_recovery_backoff PUBLIC Qt6::Core)
target_compile_features(qindaqt_recovery_backoff PUBLIC cxx_std_20)
set_target_properties(qindaqt_recovery_backoff PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_recovery_backoff)
install(TARGETS qindaqt_recovery_backoff EXPORT QindaQtTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
