# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.25)
project(QindaQtInstalledMediaProtocolConsumer LANGUAGES CXX)
find_package(Qt6 6.11 REQUIRED COMPONENTS Core)
foreach(required IN ITEMS QINDAQT_STAGE_INCLUDE_DIR QINDAQT_MEDIA_PROTOCOL_LIBRARY)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing staged-consumer input: ${required}")
    endif()
endforeach()
# Only the staged public include directory and archive enter this boundary.
add_library(InstalledMediaProtocol STATIC IMPORTED)
set_target_properties(InstalledMediaProtocol PROPERTIES
    IMPORTED_LOCATION "${QINDAQT_MEDIA_PROTOCOL_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${QINDAQT_STAGE_INCLUDE_DIR}")
target_link_libraries(InstalledMediaProtocol INTERFACE Qt6::Core)
add_executable(installed_media_protocol_consumer consumer.cpp)
target_compile_features(installed_media_protocol_consumer PRIVATE cxx_std_20)
set_target_properties(installed_media_protocol_consumer PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(installed_media_protocol_consumer PRIVATE InstalledMediaProtocol)
enable_testing()
add_test(NAME installed-media-protocol-consumer COMMAND installed_media_protocol_consumer)
