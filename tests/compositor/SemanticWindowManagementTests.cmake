# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_executable(qindaqt_semantic_fractional_maximize_tests
    tst_semanticfractionalmaximize.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridcontainerplacement.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridcontainermaximize.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridcontainershade.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridcontainerrescue.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridshadestripgeometry.cpp")
target_include_directories(qindaqt_semantic_fractional_maximize_tests PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin")
target_link_libraries(qindaqt_semantic_fractional_maximize_tests PRIVATE
    QindaQt::Hybrid QindaQt::WindowManagement QindaQt::HybridChrome
    QindaQt::HybridConstraints QindaQt::HybridInput Qt6::Test)
qindaqt_enable_warnings(qindaqt_semantic_fractional_maximize_tests)
add_test(NAME compositor.semantic-fractional-maximize COMMAND qindaqt_semantic_fractional_maximize_tests)
