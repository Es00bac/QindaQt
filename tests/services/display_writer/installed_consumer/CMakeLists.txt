# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)
project(QindaQtInstalledDisplayWriterHeaders LANGUAGES CXX)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

add_library(qindaqt_installed_display_writer_headers OBJECT
            "${QINDAQT_CONSUMER_SOURCE}")
target_compile_features(qindaqt_installed_display_writer_headers PRIVATE cxx_std_20)
set_target_properties(qindaqt_installed_display_writer_headers
                      PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(
    qindaqt_installed_display_writer_headers
    PRIVATE
        "${QINDAQT_STAGE_INCLUDE_DIR}"
        "${QINDAQT_SOURCE_ROOT}/src/services/display_service/include"
        "${QINDAQT_SOURCE_ROOT}/src/services/display_identity/include"
        "${QINDAQT_SOURCE_ROOT}/src/services/display_protocol/include"
        "${QINDAQT_SOURCE_ROOT}/src/services/display_topology/include"
        "${QINDAQT_SOURCE_ROOT}/src/services/display_transaction/include"
)
target_link_libraries(qindaqt_installed_display_writer_headers
                      PRIVATE Qt6::Core Qt6::DBus)
