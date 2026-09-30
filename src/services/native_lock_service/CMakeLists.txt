# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_library(qindaqt_native_lock_service STATIC)
add_library(QindaQt::NativeLockService ALIAS qindaqt_native_lock_service)
target_sources(qindaqt_native_lock_service PRIVATE src/qt_native_lock_request.cpp src/resident_lock_service.cpp src/manual_lock_gateway.cpp src/manual_lock_gateway_p.h src/lock_objects.cpp src/lock_objects_p.h
 PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
 include/qindaqt/services/native_lock_service/resident_lock_service.h
 include/qindaqt/services/native_lock_service/native_lock_request.h
 include/qindaqt/services/native_lock_service/qt_native_lock_request.h)
target_link_libraries(qindaqt_native_lock_service PUBLIC Qt6::Core Qt6::DBus QindaQt::CompositorAttachment QindaQt::SessionLockState PRIVATE $<BUILD_INTERFACE:qindaqt_compositor_names>)
target_compile_features(qindaqt_native_lock_service PUBLIC cxx_std_20)
qindaqt_enable_warnings(qindaqt_native_lock_service)
install(TARGETS qindaqt_native_lock_service EXPORT QindaQtTargets FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
