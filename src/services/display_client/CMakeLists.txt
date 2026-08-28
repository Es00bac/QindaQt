# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_display_client STATIC)
add_library(QindaQt::DisplayClient ALIAS qindaqt_display_client)

target_sources(
    qindaqt_display_client
    PRIVATE
        src/qt_display_transport.cpp
        src/display_client.cpp
        src/display_client_replies.cpp
        src/display_coordinator.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/display_client/display_transport.h
            include/qindaqt/services/display_client/qt_display_transport.h
            include/qindaqt/services/display_client/client.h
            include/qindaqt/services/display_client/display_coordinator.h
)

target_link_libraries(
    qindaqt_display_client
    PUBLIC
        QindaQt::DisplayProtocol
        Qt6::Core
        Qt6::DBus
)

target_compile_features(qindaqt_display_client PUBLIC cxx_std_20)
set_target_properties(qindaqt_display_client PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_display_client)

install(
    TARGETS qindaqt_display_client
    EXPORT QindaQtTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
