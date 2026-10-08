# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.25)
project(QindaQtAudioSdkStage LANGUAGES CXX)
include(GNUInstallDirs)
set(CMAKE_AUTOMOC ON)
set(QINDAQT_OBS_BRIDGE_STANDALONE OFF)
if(NOT DEFINED QINDAQT_SOURCE_ROOT)
    message(FATAL_ERROR "Production source root required")
endif()
function(qindaqt_enable_warnings target)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
endfunction()
# AGENT-GUARD: install the production FILE_SET, never copy an include tree.
add_subdirectory("${QINDAQT_SOURCE_ROOT}/src/services/audio_protocol" audio_protocol)
add_subdirectory("${QINDAQT_SOURCE_ROOT}/src/services/audio_client" audio_client)
install(EXPORT QindaQtTargets FILE AudioSdkTargets.cmake
    NAMESPACE AudioSdk:: DESTINATION lib/cmake/AudioSdk)
