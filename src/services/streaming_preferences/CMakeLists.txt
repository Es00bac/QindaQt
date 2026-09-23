# SPDX-License-Identifier: LGPL-3.0-or-later
qt_add_library(qindaqt_streaming_preferences STATIC)
add_library(QindaQt::StreamingPreferences ALIAS qindaqt_streaming_preferences)
target_sources(qindaqt_streaming_preferences PRIVATE src/settings1_streaming_preferences.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
        include/qindaqt/services/streaming_preferences/streaming_preferences.h
        include/qindaqt/services/streaming_preferences/settings1_streaming_preferences.h)
target_link_libraries(qindaqt_streaming_preferences PUBLIC QindaQt::SettingsClient Qt6::Core)
target_compile_features(qindaqt_streaming_preferences PUBLIC cxx_std_20)
set_target_properties(qindaqt_streaming_preferences PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
qindaqt_enable_warnings(qindaqt_streaming_preferences)
install(TARGETS qindaqt_streaming_preferences EXPORT QindaQtTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

# A02 scans one stable system entry. This helper reads the confirmed Settings1
# baseline, then execs OBS in-place only when login is enabled.
qt_add_executable(qindaqt-obs-login app/obs_login_main.cpp)
target_link_libraries(qindaqt-obs-login PRIVATE QindaQt::StreamingPreferences Qt6::Core)
target_compile_features(qindaqt-obs-login PRIVATE cxx_std_20)
set_target_properties(qindaqt-obs-login PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt-obs-login)
install(TARGETS qindaqt-obs-login RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT StreamingRuntime)
# Absolute sysconf destination follows the default /etc/xdg root that A02
# reads when XDG_CONFIG_DIRS is unset; DESTDIR still stages it for packaging.
install(FILES data/qindaqt-obs-login.desktop DESTINATION "/etc/xdg/autostart"
    COMPONENT StreamingRuntime)
