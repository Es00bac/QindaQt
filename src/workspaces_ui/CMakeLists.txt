# SPDX-License-Identifier: LGPL-3.0-or-later
qt_add_library(qindaqt_workspaces_ui STATIC)
add_library(QindaQt::WorkspacesUi ALIAS qindaqt_workspaces_ui)

target_sources(
    qindaqt_workspaces_ui
    PRIVATE src/workspace_dialogs.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES include/qindaqt/workspaces_ui/workspace_dialogs.h
)
target_compile_features(qindaqt_workspaces_ui PUBLIC cxx_std_20)
target_link_libraries(qindaqt_workspaces_ui PUBLIC QindaQt::Workspaces Qt6::Widgets)
set_target_properties(qindaqt_workspaces_ui PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(qindaqt_workspaces_ui)
endif()

install(TARGETS qindaqt_workspaces_ui EXPORT QindaQtTargets FILE_SET HEADERS
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
