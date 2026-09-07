# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(tst_workspace_dialogs tst_workspace_dialogs.cpp)
target_link_libraries(tst_workspace_dialogs PRIVATE QindaQt::WorkspacesUi Qt6::Test)
set_target_properties(tst_workspace_dialogs PROPERTIES AUTOMOC ON CXX_EXTENSIONS OFF)
if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(tst_workspace_dialogs)
endif()
add_test(NAME workspaces-ui.dialog-actions COMMAND tst_workspace_dialogs)
set_tests_properties(workspaces-ui.dialog-actions PROPERTIES
                     ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
