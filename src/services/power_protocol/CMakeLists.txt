# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_power_protocol STATIC)
add_library(QindaQt::PowerProtocol ALIAS qindaqt_power_protocol)

target_sources(
    qindaqt_power_protocol
    PRIVATE
        src/power_aggregation.cpp
        src/power_codec.cpp
        src/power_codec_fields.cpp
        src/power_codec_p.h
        src/power_dbus.cpp
        src/power_validation.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/power_protocol/power_aggregation.h
            include/qindaqt/services/power_protocol/power_codec.h
            include/qindaqt/services/power_protocol/power_dbus.h
            include/qindaqt/services/power_protocol/power_limits.h
            include/qindaqt/services/power_protocol/power_types.h
            include/qindaqt/services/power_protocol/power_validation.h
)
target_link_libraries(qindaqt_power_protocol PUBLIC Qt6::Core Qt6::DBus)
target_compile_features(qindaqt_power_protocol PUBLIC cxx_std_20)
set_target_properties(qindaqt_power_protocol PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_power_protocol)

install(
    TARGETS qindaqt_power_protocol
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
