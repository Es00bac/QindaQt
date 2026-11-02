# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_link_messages STATIC)
add_library(QindaQt::LinkMessages ALIAS qindaqt_link_messages)

target_sources(
    qindaqt_link_messages
    PRIVATE
        src/message_model.cpp
        src/messages_adaptor.cpp
        src/messages_feature.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/link_messages/message_model.h
            include/qindaqt/services/link_messages/messages_adaptor.h
            include/qindaqt/services/link_messages/messages_feature.h
)
target_link_libraries(
    qindaqt_link_messages
    PUBLIC QindaQt::LinkTransport Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_link_messages PUBLIC cxx_std_20)
set_target_properties(qindaqt_link_messages PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_link_messages)

install(
    TARGETS qindaqt_link_messages
    EXPORT QindaQtTargets
    ARCHIVE COMPONENT QindaQtLink
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT QindaQtLink
)
