# SPDX-License-Identifier: GPL-3.0-or-later

# The screenshot tool's Settings1 preferences and file-naming rules, shared by
# the tool (reader) and Settings → Streaming's capture section (writer).
qt_add_library(qindaqt_screenshot_preferences STATIC)
add_library(QindaQt::ScreenshotPreferences ALIAS qindaqt_screenshot_preferences)
target_sources(qindaqt_screenshot_preferences
    PRIVATE
        src/file_name_pattern.cpp
        src/settings1_screenshot_preferences.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
        include/qindaqt/services/screenshot_preferences/file_name_pattern.h
        include/qindaqt/services/screenshot_preferences/settings1_screenshot_preferences.h)
target_link_libraries(qindaqt_screenshot_preferences PUBLIC QindaQt::SettingsClient Qt6::Core)
target_compile_features(qindaqt_screenshot_preferences PUBLIC cxx_std_20)
set_target_properties(qindaqt_screenshot_preferences PROPERTIES
    CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
qindaqt_enable_warnings(qindaqt_screenshot_preferences)
install(TARGETS qindaqt_screenshot_preferences EXPORT QindaQtTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
