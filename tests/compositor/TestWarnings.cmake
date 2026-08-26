# SPDX-License-Identifier: GPL-3.0-or-later

# AGENT-GUARD: Keep the central registry focused on target construction, and
# represent every target here so Release-only defects cannot hide behind an
# executable that silently bypasses the repository warning policy.
if(COMMAND qindaqt_enable_warnings)
    foreach(compositor_test_target IN ITEMS
            qindaqt_compositor_codec_tests
            qindaqt_compositor_bridge_tests
            qindaqt_layout_geometry_tests)
        qindaqt_enable_warnings("${compositor_test_target}")
    endforeach()

    if(TARGET qindaqt_kwin_input_adapter_tests)
        foreach(kwin_test_target IN ITEMS
                qindaqt_kwin_input_adapter_tests
                qindaqt_hybrid_chrome_pointer_router_tests
                qindaqt_development_input_protocol_tests
                qindaqt_kwin_development_input_injector_tests
                qindaqt_hybrid_shortcut_manager_tests
                qindaqt_kwin_chrome_manager_tests
                qindaqt_kwin_chrome_scene_lifecycle_tests
                qindaqt_hybrid_group_stacking_tests
                qindaqt_hybrid_chrome_sync_tests
                qindaqt_hybrid_chrome_plan_builder_tests
                qindaqt_hybridmemberpolicy_tests
                qindaqt_hybridtransientpolicy_tests
                qindaqt_hybrid_chrome_drag_translator_tests
                qindaqt_hybrid_dock_target_routing_tests
                qindaqt_hybrid_chrome_exposure_tests
                qindaqt_hybrid_group_context_tests
                qindaqt_hybrid_window_admission_tests
                qindaqt_hybrid_container_placement_tests
                qindaqt_kwin_dock_preview_tests
                qindaqt_container_close_prompt_tests
                qindaqt_kwin_group_context_menu_tests
                qindaqt_hybridinteractionruntime_tests
                qindaqt_hybridinteractiontranslation_tests
                qindaqt_hybridinteractionpages_tests)
            qindaqt_enable_warnings("${kwin_test_target}")
        endforeach()
        foreach(hybrid_scene_target IN LISTS _qindaqt_hybrid_scene_test_targets)
            qindaqt_enable_warnings("${hybrid_scene_target}")
        endforeach()
    endif()
endif()
