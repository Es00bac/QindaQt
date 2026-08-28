# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.28)
project(qindaqt_installed_display_color_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core)

add_executable(qindaqt_installed_display_color_consumer "${QINDAQT_CONSUMER_SOURCE}")

target_include_directories(
    qindaqt_installed_display_color_consumer
    PRIVATE "${QINDAQT_STAGE_INCLUDE_DIR}"
)

target_link_libraries(
    qindaqt_installed_display_color_consumer
    PRIVATE "${QINDAQT_DISPLAY_COLOR_MODEL_LIBRARY}" Qt6::Core
)
