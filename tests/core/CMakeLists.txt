# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(tst_windowcontainer tst_windowcontainer.cpp)
target_compile_features(tst_windowcontainer PRIVATE cxx_std_20)
target_link_libraries(tst_windowcontainer PRIVATE QindaQt::Core Qt6::Test)
set_target_properties(tst_windowcontainer PROPERTIES
    AUTOMOC ON
    CXX_EXTENSIONS OFF
)

if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(tst_windowcontainer)
endif()

add_test(NAME core.windowcontainer COMMAND tst_windowcontainer)

add_executable(tst_reorganization tst_reorganization.cpp)
target_compile_features(tst_reorganization PRIVATE cxx_std_20)
target_link_libraries(tst_reorganization PRIVATE QindaQt::Core Qt6::Test)
set_target_properties(tst_reorganization PROPERTIES
    AUTOMOC ON
    CXX_EXTENSIONS OFF
)

if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(tst_reorganization)
endif()

add_test(NAME core.reorganization COMMAND tst_reorganization)

add_executable(tst_pagenames tst_pagenames.cpp)
target_link_libraries(tst_pagenames PRIVATE QindaQt::Core Qt6::Test)
set_target_properties(tst_pagenames PROPERTIES AUTOMOC ON)
qindaqt_enable_warnings(tst_pagenames)
add_test(NAME core.page-names COMMAND tst_pagenames)
