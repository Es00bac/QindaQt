# SPDX-License-Identifier: LGPL-3.0-or-later
qt_add_library(qindaqt_settings_client STATIC)
add_library(QindaQt::SettingsClient ALIAS qindaqt_settings_client)
target_sources(
    qindaqt_settings_client
    PRIVATE
        src/do_not_disturb_controller.cpp
        src/settings_client.cpp
        src/settings_reply_validation.cpp
        src/settings_reply_validation_p.h
        src/qt_settings_transport.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/settings_client/do_not_disturb_controller.h
            include/qindaqt/services/settings_client/settings_client.h
            include/qindaqt/services/settings_client/settings_transport.h
            include/qindaqt/services/settings_client/qt_settings_transport.h
)
target_link_libraries(qindaqt_settings_client PUBLIC QindaQt::SettingsProtocol Qt6::Core Qt6::DBus)
target_compile_features(qindaqt_settings_client PUBLIC cxx_std_20)
set_target_properties(qindaqt_settings_client PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_settings_client)
install(
    TARGETS qindaqt_settings_client EXPORT QindaQtTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
