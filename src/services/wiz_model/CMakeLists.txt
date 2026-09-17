# SPDX-License-Identifier: LGPL-3.0-or-later

# Device inventory, reachability, and revision accounting for Wiz luminaires.
# No sockets and no timers live here: the client feeds this model decoded
# datagrams and poll outcomes.
find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_wiz_model STATIC)
add_library(QindaQt::WizModel ALIAS qindaqt_wiz_model)

target_sources(
    qindaqt_wiz_model
    PRIVATE src/wiz_model.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES include/qindaqt/services/wiz_model/wiz_model.h
)
target_link_libraries(qindaqt_wiz_model PUBLIC QindaQt::WizProtocol Qt6::Core)
target_compile_features(qindaqt_wiz_model PUBLIC cxx_std_20)
set_target_properties(qindaqt_wiz_model PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_wiz_model)

install(
    TARGETS qindaqt_wiz_model
    EXPORT QindaQtTargets
    ARCHIVE COMPONENT QindaQtSmartLights
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT QindaQtSmartLights
)
