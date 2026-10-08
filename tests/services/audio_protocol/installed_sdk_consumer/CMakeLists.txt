# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.25)
project(QindaQtInstalledAudioSdkConsumer LANGUAGES CXX)
find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)
if(NOT DEFINED QINDAQT_STAGE OR NOT DEFINED QINDAQT_POISON_INCLUDE)
    message(FATAL_ERROR "Isolated stage and poison fallback required")
endif()
include("${QINDAQT_STAGE}/lib/cmake/AudioSdk/AudioSdkTargets.cmake")
add_executable(audio_sdk_consumer main.cpp)
target_compile_features(audio_sdk_consumer PRIVATE cxx_std_20)
set_target_properties(audio_sdk_consumer PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(audio_sdk_consumer PRIVATE -Wall -Wextra -Werror)
# Stage first, poison second: no missing dependency can use a host header.
target_include_directories(audio_sdk_consumer BEFORE PRIVATE
    "${QINDAQT_STAGE}/include" "${QINDAQT_POISON_INCLUDE}")
target_link_libraries(audio_sdk_consumer PRIVATE AudioSdk::qindaqt_audio_client)
