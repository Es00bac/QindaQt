# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_library(qindaqt_native_lock_runtime STATIC)
add_library(QindaQt::NativeLockRuntime ALIAS qindaqt_native_lock_runtime)
target_sources(qindaqt_native_lock_runtime PRIVATE
  src/native_lock_runtime.cpp
  PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
  include/qindaqt/session/native_lock_runtime/native_lock_runtime.h)
target_link_libraries(qindaqt_native_lock_runtime PUBLIC
  Qt6::Core QindaQt::IdleObservation QindaQt::LockPreferences
  QindaQt::NativeLockService QindaQt::SessionLockState QindaQt::PowerClient)
target_compile_features(qindaqt_native_lock_runtime PUBLIC cxx_std_20)
set_target_properties(qindaqt_native_lock_runtime PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_native_lock_runtime)
install(TARGETS qindaqt_native_lock_runtime EXPORT QindaQtTargets FILE_SET HEADERS
  DESTINATION "\${CMAKE_INSTALL_INCLUDEDIR}")
