# SPDX-License-Identifier: GPL-3.0-or-later
#
# AGENT-NOTE: its own file because tests/compositor/CMakeLists.txt is at
# the 600-line shape limit, the same reason the member-policy and
# chrome-badge suites live in their own includes.

# Whole-container roll-up, split from the placement suite the same way the
# production code is (hybridcontainershade.cpp): one behaviour, one file.
qt_add_executable(
    qindaqt_hybrid_container_shade_tests
    tst_hybridcontainershade.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridcontainerplacement.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridcontainershade.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridshadestripgeometry.cpp"
)
target_include_directories(
    qindaqt_hybrid_container_shade_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
)
target_link_libraries(
    qindaqt_hybrid_container_shade_tests
    PRIVATE
        QindaQt::Hybrid
        QindaQt::HybridChrome
        QindaQt::HybridConstraints
        QindaQt::HybridInput
        Qt6::Test
)
add_test(
    NAME compositor.hybrid-container-shade
    COMMAND qindaqt_hybrid_container_shade_tests
)
