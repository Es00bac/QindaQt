# SPDX-License-Identifier: LGPL-3.0-or-later

add_library(qindaqt_core STATIC
    layoutnode.cpp
    layoutnode_json.cpp
    windowcontainer.cpp
    windowcontainer_json.cpp
)
add_library(QindaQt::Core ALIAS qindaqt_core)

target_compile_features(qindaqt_core PUBLIC cxx_std_20)
target_include_directories(qindaqt_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qindaqt_core PUBLIC Qt6::Core)

set_target_properties(qindaqt_core PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(qindaqt_core)
endif()
