# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_link_focus STATIC)
add_library(QindaQt::LinkFocus ALIAS qindaqt_link_focus)

target_sources(
    qindaqt_link_focus
    PRIVATE
        src/focus_store.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/link_focus/focus_source.h
            include/qindaqt/services/link_focus/focus_store.h
)
target_link_libraries(qindaqt_link_focus PUBLIC Qt6::Core)
target_compile_features(qindaqt_link_focus PUBLIC cxx_std_20)
set_target_properties(qindaqt_link_focus PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_link_focus)

install(
    TARGETS qindaqt_link_focus
    EXPORT QindaQtTargets
    ARCHIVE COMPONENT QindaQtLink
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT QindaQtLink
)
