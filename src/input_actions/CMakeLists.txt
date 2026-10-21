# SPDX-License-Identifier: GPL-3.0-or-later
add_library(qindaqt_input_actions STATIC action_ids.cpp binding_codec.cpp macro_codec.cpp macro_store.cpp macro_runner.cpp
    include/qindaqt/input_actions/macro_runner.h)
add_library(QindaQt::InputActions ALIAS qindaqt_input_actions)
target_include_directories(qindaqt_input_actions PUBLIC include)
target_link_libraries(qindaqt_input_actions PUBLIC Qt6::Core Qt6::Gui)
target_compile_features(qindaqt_input_actions PUBLIC cxx_std_20)
set_target_properties(qindaqt_input_actions PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(qindaqt_input_actions)
endif()
