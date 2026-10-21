# SPDX-License-Identifier: GPL-3.0-or-later
add_library(qindaqt_controller_policy STATIC controller_policy.cpp button_layers.cpp profile_codec.cpp text_delivery.cpp text_delivery_p.h profile_store.cpp game_priority.cpp held_input_state.cpp)
add_library(QindaQt::ControllerPolicy ALIAS qindaqt_controller_policy)
target_include_directories(qindaqt_controller_policy PUBLIC include)
target_link_libraries(qindaqt_controller_policy PUBLIC Qt6::Core Qt6::Gui QindaQt::InputActions)
target_compile_features(qindaqt_controller_policy PUBLIC cxx_std_20)
set_target_properties(qindaqt_controller_policy PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(QINDAQT_BUILD_KWIN_PLUGIN)
    find_package(QindaQtKWin 6.6.6.1 EXACT REQUIRED CONFIG)
    find_package(SDL3 3.2 REQUIRED CONFIG)
    add_library(qindaqt_controller_runtime STATIC controller_runtime.cpp
        game_priority_observer.cpp game_priority_observer_p.h
        include/qindaqt/controllers/controller_runtime.h)
    target_link_libraries(qindaqt_controller_runtime PUBLIC qindaqt_controller_policy SDL3::SDL3)
    set_target_properties(qindaqt_controller_runtime PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_library(qindaqt_controller_plugin MODULE
        kwin/main.cpp kwin/controller_plugin.cpp kwin/controller_plugin.h
        kwin/controller_input_device.cpp kwin/controller_input_device.h
        kwin/voice_button.cpp kwin/voice_button.h
        kwin/controller_text_output.cpp kwin/controller_text_output.h
        kwin/macro_endpoint.cpp kwin/macro_endpoint.h
        kwin/macro_triggers.cpp kwin/macro_triggers.h)
    target_link_libraries(qindaqt_controller_plugin PRIVATE qindaqt_controller_runtime QindaQt::ShortcutsClient QindaQtKWin::kwin Qt6::DBus Qt6::Widgets)
    target_compile_features(qindaqt_controller_plugin PRIVATE cxx_std_23)
    set_target_properties(qindaqt_controller_plugin PROPERTIES PREFIX "" OUTPUT_NAME "qindaqt_controllers")
    install(TARGETS qindaqt_controller_plugin LIBRARY DESTINATION "${QT6_INSTALL_PLUGINS}/qindaqt-kwin/plugins" COMPONENT ControllerRuntime)
endif()
if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(qindaqt_controller_policy)
endif()
