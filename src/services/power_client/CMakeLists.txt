# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_power_client STATIC)
add_library(QindaQt::PowerClient ALIAS qindaqt_power_client)

target_sources(
    qindaqt_power_client
    PRIVATE
        src/power_client.cpp
        src/power_client_completion.cpp
        src/power_client_operations.cpp
        src/qt_power_transport.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/power_client/power_client.h
            include/qindaqt/services/power_client/power_transport.h
            include/qindaqt/services/power_client/qt_power_transport.h
)
target_link_libraries(
    qindaqt_power_client PUBLIC QindaQt::PowerProtocol Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_power_client PUBLIC cxx_std_20)
set_target_properties(qindaqt_power_client PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_power_client)

install(
    TARGETS qindaqt_power_client
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
