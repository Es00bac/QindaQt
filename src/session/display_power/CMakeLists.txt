# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_library(qindaqt_display_power STATIC)
add_library(QindaQt::DisplayPower ALIAS qindaqt_display_power)
target_sources(qindaqt_display_power PRIVATE src/scoped_display_power.cpp src/display_power_facade.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
    include/qindaqt/session/display_power/scoped_display_power.h
    include/qindaqt/session/display_power/display_power_facade.h)
target_link_libraries(qindaqt_display_power PUBLIC Qt6::Core Qt6::DBus QindaQt::IdlePolicy
    QindaQt::NativeLockRuntime QindaQt::PowerClient PRIVATE $<BUILD_INTERFACE:qindaqt_compositor_names>)
target_compile_features(qindaqt_display_power PUBLIC cxx_std_20)
set_target_properties(qindaqt_display_power PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_display_power)
install(TARGETS qindaqt_display_power EXPORT QindaQtTargets FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
