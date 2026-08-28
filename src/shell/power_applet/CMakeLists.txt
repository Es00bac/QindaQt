# SPDX-License-Identifier: LGPL-3.0-or-later

# Presentation-only Power applet model. AGENT-NOTE: pure presentation projection
# and request state machine. Do not add product code here that reaches beyond
# public PB-0 values (power_protocol, brightness_model) and Qt Core.

qt_add_library(qindaqt_shell_power_applet STATIC)
add_library(QindaQt::ShellPowerApplet ALIAS qindaqt_shell_power_applet)

target_sources(
    qindaqt_shell_power_applet
    PRIVATE
        src/brightness_request_state.cpp
        src/power_applet_presentation.cpp
        src/power_control_rows.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/power_applet/brightness_request_state.h
            include/qindaqt/shell/power_applet/power_applet_presentation.h
            include/qindaqt/shell/power_applet/power_applet_types.h
)
target_link_libraries(
    qindaqt_shell_power_applet
    PUBLIC QindaQt::PowerProtocol QindaQt::BrightnessModel Qt6::Core
)
target_compile_features(qindaqt_shell_power_applet PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_power_applet PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_power_applet)
