# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(KF6Config 6.0 REQUIRED CONFIG)

qt_add_library(qindaqt_power_policy STATIC)
add_library(QindaQt::PowerPolicy ALIAS qindaqt_power_policy)
target_sources(qindaqt_power_policy
    PRIVATE src/powerdevil_import.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
        include/qindaqt/services/power_policy/powerdevil_import.h
)
target_link_libraries(qindaqt_power_policy PUBLIC KF6::ConfigCore Qt6::Core)
target_compile_features(qindaqt_power_policy PUBLIC cxx_std_20)
set_target_properties(qindaqt_power_policy PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_power_policy)
