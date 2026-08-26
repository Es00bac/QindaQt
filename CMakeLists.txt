# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)

project(
    QindaQt
    VERSION 0.1.0
    DESCRIPTION "A modular Qt desktop environment with hybrid window containers"
    LANGUAGES CXX
)

include(CTest)

# QindaQt ships a binary KWin plugin, so use KDE's Qt-aware install layout
# globally. Including GNUInstallDirs first makes ECM inherit ambiguous values
# and can silently turn plugin destinations into absolute root paths.
find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")
include(KDEInstallDirs6)

option(BUILD_SHARED_LIBS "Build loadable integration modules as shared libraries" ON)
option(QINDAQT_BUILD_SHELL "Build QindaQt shell presentation and preview" ON)
option(QINDAQT_BUILD_PRODUCTION_SHELL
       "Build the LayerShellQt-backed production Wayland shell"
       "${QINDAQT_BUILD_SHELL}")
option(QINDAQT_BUILD_KWIN_PLUGIN
       "Require and build the release-matched KWin integration plugin" ON)
option(QINDAQT_ENABLE_STRICT_WARNINGS "Treat QindaQt warnings as errors" ON)
option(QINDAQT_ENABLE_HOST_UINPUT_TESTS
       "Register tests that may inject pointer/keyboard events into the host seat" OFF)

if(QINDAQT_ENABLE_HOST_UINPUT_TESTS)
    message(WARNING
        "Host uinput tests are enabled. They can move, click, and type on the active "
        "desktop; run them only on a dedicated virtual seat or disposable machine.")
endif()

if(QINDAQT_BUILD_PRODUCTION_SHELL AND NOT QINDAQT_BUILD_SHELL)
    message(FATAL_ERROR
        "QINDAQT_BUILD_PRODUCTION_SHELL requires QINDAQT_BUILD_SHELL")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(QindaQtCompiler)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Widgets)
if(BUILD_TESTING)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Test)
endif()

qt_standard_project_setup(REQUIRES 6.11)

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
