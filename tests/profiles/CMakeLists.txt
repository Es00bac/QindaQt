# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_profile_tests tst_profiles.cpp)
target_link_libraries(qindaqt_profile_tests PRIVATE qindaqt_profiles Qt6::Test)
target_compile_definitions(qindaqt_profile_tests PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
qindaqt_enable_warnings(qindaqt_profile_tests)
add_test(NAME qindaqt.profile-formats COMMAND qindaqt_profile_tests)
