# SPDX-License-Identifier: GPL-3.0-or-later
# Shared by the compositor and its display-free focused fixture. No KWin ABI
# dependency enters the private transport or its QtCore-only protocol target.
qt_add_library(qindaqt_android_renderer_evidence STATIC)
add_library(QindaQt::AndroidRendererEvidence ALIAS qindaqt_android_renderer_evidence)
target_sources(qindaqt_android_renderer_evidence PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/android_renderer_evidence.cpp"
    PUBLIC FILE_SET HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../include" FILES
        "${CMAKE_CURRENT_LIST_DIR}/../include/qindaqt/compositor/android_renderer_evidence.h")
target_link_libraries(qindaqt_android_renderer_evidence PUBLIC QindaQt::AndroidRuntime Qt6::Core)
target_compile_features(qindaqt_android_renderer_evidence PUBLIC cxx_std_20)
set_target_properties(qindaqt_android_renderer_evidence PROPERTIES
    POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_android_renderer_evidence)
install(TARGETS qindaqt_android_renderer_evidence EXPORT QindaQtTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
