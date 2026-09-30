# SPDX-License-Identifier: GPL-3.0-or-later
#
# AGENT-NOTE: its own file for the same reason as ContainerShadeTests.cmake:
# tests/compositor/CMakeLists.txt is past its shape budget.

# Leaving whole-container maximize by moving, resizing, or rolling up
# (ADR-0282), split from the placement suite the way the production code is
# (hybridcontainermaximize.cpp): one behaviour, one file.
qt_add_executable(
    qindaqt_hybrid_container_maximize_tests
    tst_hybridcontainermaximize.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridcontainerplacement.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridcontainermaximize.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridcontainershade.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridcontainerrescue.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridshadestripgeometry.cpp"
)
target_include_directories(
    qindaqt_hybrid_container_maximize_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
)
target_link_libraries(
    qindaqt_hybrid_container_maximize_tests
    PRIVATE
        QindaQt::Hybrid
        QindaQt::WindowManagement
        QindaQt::HybridChrome
        QindaQt::HybridConstraints
        QindaQt::HybridInput
        Qt6::Test
)
add_test(
    NAME compositor.hybrid-container-maximize
    COMMAND qindaqt_hybrid_container_maximize_tests
)
