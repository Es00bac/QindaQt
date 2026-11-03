# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_link_client STATIC)
add_library(QindaQt::LinkClient ALIAS qindaqt_link_client)

target_sources(
    qindaqt_link_client
    PRIVATE
        src/link_client.cpp
        src/qt_link_transport.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/link_client/link_client.h
            include/qindaqt/services/link_client/link_transport.h
            include/qindaqt/services/link_client/qt_link_transport.h
)
target_link_libraries(qindaqt_link_client PUBLIC Qt6::Core Qt6::DBus)
target_compile_features(qindaqt_link_client PUBLIC cxx_std_20)
set_target_properties(qindaqt_link_client PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_link_client)

# AGENT-GUARD: install only when a Link program is part of this build (the gui-apps/qindalink
# package). The desktop package builds link_client for the panel applet alone and must ship no
# Link file; installing it there would collide with gui-apps/qindalink.
if(QINDAQT_BUILD_LINK_SERVICE OR QINDAQT_BUILD_LINK_APP)
    install(
        TARGETS qindaqt_link_client
        EXPORT QindaQtTargets
        ARCHIVE COMPONENT QindaQtLink
        FILE_SET HEADERS
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        COMPONENT QindaQtLink
    )
endif()
