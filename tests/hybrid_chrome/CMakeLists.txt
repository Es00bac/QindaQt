# SPDX-License-Identifier: GPL-3.0-or-later

# This module can configure alone for focused agent work. Repository builds add
# src/hybrid_chrome and this directory from their normal shared registries.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtHybridChromeTests LANGUAGES CXX)
    include(CTest)
    include(GNUInstallDirs)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
    include(QindaQtCompiler)
    set(QINDAQT_ENABLE_STRICT_WARNINGS ON)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Test Widgets)
    qt_standard_project_setup(REQUIRES 6.11)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../src/hybrid_chrome"
                     "${CMAKE_CURRENT_BINARY_DIR}/hybrid_chrome")
endif()

find_package(Qt6 6.11 REQUIRED COMPONENTS Test)

foreach(test_name IN ITEMS layout hittest renderer)
    qt_add_executable(qindaqt_hybrid_chrome_${test_name}_tests tst_${test_name}.cpp)
    target_link_libraries(
        qindaqt_hybrid_chrome_${test_name}_tests
        PRIVATE QindaQt::HybridChrome Qt6::Test
    )
    target_compile_features(qindaqt_hybrid_chrome_${test_name}_tests PRIVATE cxx_std_20)
    set_target_properties(
        qindaqt_hybrid_chrome_${test_name}_tests
        PROPERTIES CXX_EXTENSIONS OFF
    )
    if(COMMAND qindaqt_enable_warnings)
        qindaqt_enable_warnings(qindaqt_hybrid_chrome_${test_name}_tests)
    endif()
    add_test(
        NAME hybrid-chrome.${test_name}
        COMMAND qindaqt_hybrid_chrome_${test_name}_tests
    )
endforeach()

set_tests_properties(
    hybrid-chrome.renderer
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
