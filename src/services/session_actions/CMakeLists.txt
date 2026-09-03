# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_session_actions STATIC)
add_library(QindaQt::SessionActions ALIAS qindaqt_session_actions)

target_sources(
    qindaqt_session_actions
    PRIVATE src/session_actions_client.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/session_actions/session_actions_client.h
)
target_link_libraries(qindaqt_session_actions PUBLIC Qt6::Core Qt6::DBus)
target_compile_features(qindaqt_session_actions PUBLIC cxx_std_20)
set_target_properties(qindaqt_session_actions PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_session_actions)

install(
    TARGETS qindaqt_session_actions
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
