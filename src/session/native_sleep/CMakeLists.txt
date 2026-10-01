# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_library(qindaqt_native_sleep STATIC)
add_library(QindaQt::NativeSleep ALIAS qindaqt_native_sleep)
target_sources(qindaqt_native_sleep PRIVATE
  src/logind_sleep_transport.cpp src/sleep_coordinator.cpp src/sleep_service.cpp
  PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
  include/qindaqt/session/native_sleep/logind_sleep_transport.h
  include/qindaqt/session/native_sleep/sleep_coordinator.h
  include/qindaqt/session/native_sleep/sleep_service.h)
target_link_libraries(qindaqt_native_sleep PUBLIC Qt6::Core Qt6::DBus
  QindaQt::NativeLockRuntime QindaQt::SessionLockState)
target_compile_features(qindaqt_native_sleep PUBLIC cxx_std_20)
set_target_properties(qindaqt_native_sleep PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_native_sleep)
install(TARGETS qindaqt_native_sleep EXPORT QindaQtTargets FILE_SET HEADERS
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
