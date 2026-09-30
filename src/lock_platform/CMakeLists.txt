# SPDX-License-Identifier: GPL-3.0-or-later
add_library(qindaqt_lock_platform STATIC process_protection.cpp process_protection.h)
target_include_directories(qindaqt_lock_platform PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qindaqt_lock_platform PUBLIC Qt6::Core)
qindaqt_enable_warnings(qindaqt_lock_platform)
