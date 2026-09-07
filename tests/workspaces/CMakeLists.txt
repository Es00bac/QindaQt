# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 REQUIRED COMPONENTS Test)
add_executable(tst_workspaces tst_workspaces.cpp)
target_link_libraries(tst_workspaces PRIVATE QindaQt::Workspaces Qt6::Test)
set_target_properties(tst_workspaces PROPERTIES AUTOMOC ON CXX_EXTENSIONS OFF)
if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(tst_workspaces)
endif()
add_test(NAME workspaces.persistence-assignment COMMAND tst_workspaces)
