# SPDX-License-Identifier: GPL-3.0-or-later

# This conditional makes the focused suite runnable before the parent build
# files adopt this new module. It is inert once integrated by tests/CMakeLists.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtHybridTests LANGUAGES CXX)
    include(CTest)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test)
    set(QINDAQT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
    add_subdirectory("${QINDAQT_ROOT}/src/core" "${CMAKE_BINARY_DIR}/src-core")
    add_subdirectory("${QINDAQT_ROOT}/src/hybrid" "${CMAKE_BINARY_DIR}/src-hybrid")
endif()

function(qindaqt_add_hybrid_test target source)
    add_executable(${target} ${source})
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_link_libraries(${target} PRIVATE QindaQt::Hybrid Qt6::Test)
    set_target_properties(${target} PROPERTIES AUTOMOC ON CXX_EXTENSIONS OFF)
    if(COMMAND qindaqt_enable_warnings)
        qindaqt_enable_warnings(${target})
    endif()
    add_test(NAME hybrid.${target} COMMAND ${target})
endfunction()

qindaqt_add_hybrid_test(topology_commands tst_topology_commands.cpp)
qindaqt_add_hybrid_test(topology_placement tst_topology_placement.cpp)
qindaqt_add_hybrid_test(atomic_coordinator tst_atomic_coordinator.cpp)
