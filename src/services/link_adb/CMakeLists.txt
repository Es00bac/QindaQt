# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_link_adb STATIC)
add_library(QindaQt::LinkAdb ALIAS qindaqt_link_adb)

target_sources(
    qindaqt_link_adb
    PRIVATE
        src/qprocess_adb_runner.cpp
        src/tunnel_manager.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/link_adb/adb_runner.h
            include/qindaqt/services/link_adb/tunnel_manager.h
)
target_link_libraries(qindaqt_link_adb PUBLIC Qt6::Core)
target_compile_features(qindaqt_link_adb PUBLIC cxx_std_20)
set_target_properties(qindaqt_link_adb PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_link_adb)

install(
    TARGETS qindaqt_link_adb
    EXPORT QindaQtTargets
    ARCHIVE COMPONENT QindaQtLink
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT QindaQtLink
)
