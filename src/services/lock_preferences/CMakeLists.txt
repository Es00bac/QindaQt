# SPDX-License-Identifier: LGPL-3.0-or-later
qt_add_library(qindaqt_lock_preferences STATIC)
add_library(QindaQt::LockPreferences ALIAS qindaqt_lock_preferences)
target_sources(qindaqt_lock_preferences PRIVATE
    src/lock_preferences.cpp src/legacy_lock_import.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
    include/qindaqt/services/lock_preferences/lock_preferences.h
    include/qindaqt/services/lock_preferences/legacy_lock_import.h)
target_link_libraries(qindaqt_lock_preferences PUBLIC Qt6::Core QindaQt::SettingsClient)
target_compile_features(qindaqt_lock_preferences PUBLIC cxx_std_20)
set_target_properties(qindaqt_lock_preferences PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_lock_preferences)
install(TARGETS qindaqt_lock_preferences EXPORT QindaQtTargets FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
