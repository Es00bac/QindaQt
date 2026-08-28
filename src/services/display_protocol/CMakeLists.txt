# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_display_protocol STATIC)
add_library(QindaQt::DisplayProtocol ALIAS qindaqt_display_protocol)

target_sources(
    qindaqt_display_protocol
    PRIVATE
        src/display_codec.cpp
        src/display_codec_p.h
        src/display_codec_snapshot.cpp
        src/display_dbus.cpp
        src/display_validation.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/display_protocol/display_codec.h
            include/qindaqt/services/display_protocol/display_dbus.h
            include/qindaqt/services/display_protocol/display_limits.h
            include/qindaqt/services/display_protocol/display_types.h
            include/qindaqt/services/display_protocol/display_validation.h
)
target_link_libraries(qindaqt_display_protocol PUBLIC Qt6::Core Qt6::DBus)
target_compile_features(qindaqt_display_protocol PUBLIC cxx_std_20)
set_target_properties(qindaqt_display_protocol PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_display_protocol)

install(
    TARGETS qindaqt_display_protocol
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
