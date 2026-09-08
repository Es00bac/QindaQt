# SPDX-License-Identifier: LGPL-3.0-or-later
# QPA is the only private Qt boundary; rebuild this plugin with each Qt update.
find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
if(NOT TARGET Qt6::GuiPrivate)
    message(FATAL_ERROR "QindaQt's Qt platform theme requires matching Qt Gui private development headers")
endif()
add_library(qindaqt_native_palette STATIC native_palette.cpp native_palette.h)
target_link_libraries(qindaqt_native_palette PUBLIC QindaQt::DesignTokens Qt6::Gui)
target_include_directories(qindaqt_native_palette PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(qindaqt_native_palette PUBLIC cxx_std_20)
set_target_properties(qindaqt_native_palette PROPERTIES POSITION_INDEPENDENT_CODE ON)
qindaqt_enable_warnings(qindaqt_native_palette)
add_library(qindaqt_qt_platform_theme MODULE plugin.cpp qindaqt_platform_theme.cpp qindaqt_platform_theme.h qindaqt.json)
target_link_libraries(qindaqt_qt_platform_theme PRIVATE qindaqt_native_palette QindaQt::AppAppearance Qt6::GuiPrivate Qt6::DBus)
target_compile_features(qindaqt_qt_platform_theme PRIVATE cxx_std_20)
set_target_properties(qindaqt_qt_platform_theme PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/platformthemes")
qindaqt_enable_warnings(qindaqt_qt_platform_theme)
install(TARGETS qindaqt_qt_platform_theme
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/qt6/plugins/platformthemes" COMPONENT QtPlatformTheme)
