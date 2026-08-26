# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)

project(
    QindaQt
    VERSION 0.1.0
    DESCRIPTION "A modular Qt desktop environment with hybrid window containers"
    LANGUAGES CXX
)

include(CTest)
include(GNUInstallDirs)

option(QINDAQT_BUILD_SHELL "Build the interactive shell preview" ON)
option(QINDAQT_ENABLE_STRICT_WARNINGS "Treat QindaQt warnings as errors" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(QindaQtCompiler)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
if(BUILD_TESTING)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Test)
endif()

qt_standard_project_setup(REQUIRES 6.11)

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
