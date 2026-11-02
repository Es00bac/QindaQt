# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_link_protocol STATIC)
add_library(QindaQt::LinkProtocol ALIAS qindaqt_link_protocol)

target_sources(
    qindaqt_link_protocol
    PRIVATE
        src/envelope.cpp
        src/frame_codec.cpp
        src/pairing_math.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/link_protocol/envelope.h
            include/qindaqt/services/link_protocol/frame_codec.h
            include/qindaqt/services/link_protocol/pairing_math.h
)
target_link_libraries(qindaqt_link_protocol PUBLIC Qt6::Core)
target_compile_features(qindaqt_link_protocol PUBLIC cxx_std_20)
set_target_properties(qindaqt_link_protocol PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_link_protocol)

install(
    TARGETS qindaqt_link_protocol
    EXPORT QindaQtTargets
    ARCHIVE COMPONENT QindaQtLink
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT QindaQtLink
)
