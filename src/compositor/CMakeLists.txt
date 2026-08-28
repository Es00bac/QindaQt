# SPDX-License-Identifier: LGPL-3.0-or-later

add_library(qindaqt_compositor_bridge STATIC)
add_library(QindaQt::CompositorBridge ALIAS qindaqt_compositor_bridge)

target_sources(
    qindaqt_compositor_bridge
    PRIVATE
        src/containercontrolbridge.cpp
        src/controlcodec.cpp
        src/controlendpoint.cpp
        src/operationexecutor.cpp
        src/shellvisibilitysnapshot.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/compositor/containercontrolbridge.h
            include/qindaqt/compositor/controlcodec.h
            include/qindaqt/compositor/controlendpoint.h
            include/qindaqt/compositor/controllimits.h
            include/qindaqt/compositor/controltypes.h
            include/qindaqt/compositor/sceneadapter.h
            include/qindaqt/compositor/shellvisibilitysnapshot.h
)

target_compile_features(qindaqt_compositor_bridge PUBLIC cxx_std_20)
target_link_libraries(
    qindaqt_compositor_bridge
    PUBLIC QindaQt::Core QindaQt::ShellVisibilityProtocol Qt6::Core
)
set_target_properties(
    qindaqt_compositor_bridge
    PROPERTIES
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
)

if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(qindaqt_compositor_bridge)
endif()

option(QINDAQT_BUILD_KWIN_PLUGIN "Require and build the release-matched KWin integration plugin" ON)
include(cmake/QindaQtKWinAbi.cmake)
qindaqt_resolve_kwin_abi(
    "${CMAKE_CURRENT_SOURCE_DIR}/../../compositor/upstream/kwin.json"
    _qindaqt_kwin_abi_version
)

if(QINDAQT_BUILD_KWIN_PLUGIN)
    # KWin 6.6 exports Qt6::Widgets in its link interface but its package file
    # does not discover that component itself. Resolve it before importing KWin.
    # AGENT-GUARD: ON is a release contract, not a best-effort request. A build
    # without the exact binary ABI would install a launcher that can never
    # provide QindaQt's compositor service. Packagers must choose explicit OFF
    # when intentionally building only the persistence-neutral bridge.
    find_package(Qt6 6.11 REQUIRED COMPONENTS DBus Widgets)
    find_package(KDecoration3 6.6 REQUIRED CONFIG)
    find_package(KF6GlobalAccel 6.0 REQUIRED CONFIG)
    find_package(KWin ${_qindaqt_kwin_abi_version} EXACT REQUIRED CONFIG)
    # KWin's public Activities header includes PlasmaActivities but its package
    # does not propagate that include directory to extension plugins.
    find_package(PlasmaActivities ${_qindaqt_kwin_abi_version} EXACT REQUIRED CONFIG)

    add_library(qindaqt_hybrid_chrome_pointer_router STATIC
        kwin/hybridchromepointerrouter.cpp
        kwin/hybridchromepointerrouter.h
    )
    target_compile_features(qindaqt_hybrid_chrome_pointer_router PUBLIC cxx_std_20)
    target_include_directories(
        qindaqt_hybrid_chrome_pointer_router
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/kwin"
    )
    target_link_libraries(
        qindaqt_hybrid_chrome_pointer_router
        PUBLIC QindaQt::HybridChrome QindaQt::HybridInput Qt6::Core
    )
    set_target_properties(
        qindaqt_hybrid_chrome_pointer_router
        PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
    )
    if(COMMAND qindaqt_enable_warnings)
        qindaqt_enable_warnings(qindaqt_hybrid_chrome_pointer_router)
    endif()

    add_library(qindaqt_container_close_prompt STATIC
        kwin/containercloseprompt.cpp
        kwin/containercloseprompt.h
    )
    target_compile_features(qindaqt_container_close_prompt PUBLIC cxx_std_20)
    target_include_directories(
        qindaqt_container_close_prompt
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/kwin"
    )
    target_link_libraries(qindaqt_container_close_prompt PUBLIC Qt6::Widgets)
    set_target_properties(
        qindaqt_container_close_prompt
        PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
    )
    if(COMMAND qindaqt_enable_warnings)
        qindaqt_enable_warnings(qindaqt_container_close_prompt)
    endif()

    add_library(qindaqt_group_context_menu STATIC
        kwin/kwingroupcontextmenu.cpp
        kwin/kwingroupcontextmenu.h
    )
    target_compile_features(qindaqt_group_context_menu PUBLIC cxx_std_20)
    target_include_directories(
        qindaqt_group_context_menu
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/kwin"
    )
    target_link_libraries(qindaqt_group_context_menu PUBLIC Qt6::Widgets)
    set_target_properties(
        qindaqt_group_context_menu
        PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
    )
    if(COMMAND qindaqt_enable_warnings)
        qindaqt_enable_warnings(qindaqt_group_context_menu)
    endif()

    add_library(qindaqt_kwin_input_adapter STATIC)
    target_sources(
        qindaqt_kwin_input_adapter
        PRIVATE
            kwin/inputcapabilities.cpp
            kwin/inputcapabilities.h
            kwin/kwininteractionfilter.cpp
            kwin/kwininteractionfilter.h
            kwin/kwininputadapter.cpp
            kwin/kwininputadapter.h
            kwin/normalizedinputevent.cpp
            kwin/normalizedinputevent.h
    )
    target_compile_features(qindaqt_kwin_input_adapter PUBLIC cxx_std_20)
    target_include_directories(
        qindaqt_kwin_input_adapter
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/kwin"
    )
    target_link_libraries(
        qindaqt_kwin_input_adapter
        PUBLIC
            KWin::kwin
            QindaQt::HybridInput
            qindaqt_hybrid_chrome_pointer_router
            Qt6::Core
    )
    set_target_properties(
        qindaqt_kwin_input_adapter
        PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
    )
    if(COMMAND qindaqt_enable_warnings)
        qindaqt_enable_warnings(qindaqt_kwin_input_adapter)
    endif()

    add_library(qindaqt_kwin_development_input STATIC
        kwin/developmentinputprotocol.cpp
        kwin/developmentinputprotocol.h
        kwin/kwindevelopmentinputinjector.cpp
        kwin/kwindevelopmentinputinjector.h
    )
    target_compile_features(qindaqt_kwin_development_input PUBLIC cxx_std_20)
    target_include_directories(
        qindaqt_kwin_development_input
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/kwin"
    )
    target_link_libraries(
        qindaqt_kwin_development_input
        PUBLIC QindaQt::CompositorBridge KWin::kwin Qt6::Core
    )
    set_target_properties(
        qindaqt_kwin_development_input
        PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
    )
    if(COMMAND qindaqt_enable_warnings)
        qindaqt_enable_warnings(qindaqt_kwin_development_input)
    endif()

    add_library(qindaqt_kwin_output_support STATIC
        kwin/kwindevelopmentoutputseam.cpp
        kwin/kwindevelopmentoutputseam.h
        kwin/kwinoutputinventory.cpp
        kwin/kwinoutputinventory.h
    )
    target_compile_features(qindaqt_kwin_output_support PUBLIC cxx_std_20)
    target_include_directories(
        qindaqt_kwin_output_support
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/kwin"
    )
    target_link_libraries(
        qindaqt_kwin_output_support
        PUBLIC QindaQt::CompositorBridge KWin::kwin Qt6::Core
    )
    set_target_properties(
        qindaqt_kwin_output_support
        PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
    )
    if(COMMAND qindaqt_enable_warnings)
        qindaqt_enable_warnings(qindaqt_kwin_output_support)
    endif()

    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins")
    kcoreaddons_add_plugin(
        qindaqt_compositor
        SOURCES
            kwin/kwincontrolendpoint.cpp
            kwin/kwincontrolendpoint.h
            kwin/kwinshellvisibilitypublisher.cpp
            kwin/kwinshellvisibilitypublisher.h
            kwin/kwininteractiontargetresolver.cpp
            kwin/kwininteractiontargetresolver.h
            kwin/hybriddocktargetrouting.cpp
            kwin/hybriddocktargetrouting.h
            kwin/hybridchromeexposure.cpp
            kwin/hybridchromeexposure.h
            kwin/hybridgroupcontext.cpp
            kwin/hybridgroupcontext.h
            kwin/hybridwindowadmission.cpp
            kwin/hybridwindowadmission.h
            kwin/hybridshortcutmanager.cpp
            kwin/hybridshortcutmanager.h
            kwin/hybridinteractionruntime.cpp
            kwin/hybridinteractionruntime.h
            kwin/hybridinteractionplacement.cpp
            kwin/hybridchromedragtranslator.cpp
            kwin/hybridchromedragtranslator.h
            kwin/hybridchromeaccessibility.cpp
            kwin/hybridchromeaccessibility.h
            kwin/hybridchromeaccessibility_p.h
            kwin/hybridchromeaccessibilitymodel.cpp
            kwin/hybridchromeaccessibleinterface.cpp
            kwin/hybridchromeaccessibilityregistry.cpp
            kwin/hybridchromeaccessibilityregistry.h
            kwin/hybridchromesyncscheduler.cpp
            kwin/hybridchromesyncscheduler.h
            kwin/hybridchromeplanbuilder.cpp
            kwin/hybridchromeplanbuilder.h
            kwin/hybridcontainerplacement.cpp
            kwin/hybridcontainerplacement.h
            kwin/hybridmemberpolicy.cpp
            kwin/hybridmemberpolicy.h
            kwin/hybridsemanticcommand.cpp
            kwin/hybridsemanticcommand.h
            kwin/hybridshutdownrecovery.cpp
            kwin/hybridshutdownrecovery.h
            kwin/hybridtaskidentitypolicy.cpp
            kwin/hybridtaskidentitypolicy.h
            kwin/hybridtransientpolicy.cpp
            kwin/hybridtransientpolicy.h
            kwin/hybridgroupstackingpolicy.cpp
            kwin/hybridgroupstackingpolicy.h
            kwin/kwinchromemanager.cpp
            kwin/kwinchromemanager.h
            kwin/kwinchromehitrouting.cpp
            kwin/kwinchromescenelifecycle.cpp
            kwin/kwinchromescenelifecycle.h
            kwin/kwinscenechromeoverlay.cpp
            kwin/kwinscenechromeoverlay.h
            kwin/kwindockpreview.cpp
            kwin/kwindockpreview.h
            kwin/kwingroupcontextmanager.cpp
            kwin/kwingroupcontextmanager.h
            kwin/kwinhybridplatform.cpp
            kwin/kwinhybridrecovery.cpp
            kwin/kwinhybridreflow.cpp
            kwin/kwinhybridscene.cpp
            kwin/kwinhybridscene.h
            kwin/kwinhybridgroupstacking.cpp
            kwin/kwinhybridgroupstacking.h
            kwin/kwinhybridcontext.cpp
            kwin/kwinhybridcontextmenu.cpp
            kwin/kwinhybridinteraction.cpp
            kwin/kwinhybridsession.cpp
            kwin/kwinhybridsession.h
            kwin/kwinhybridshutdown.cpp
            kwin/kwinhybridshutdown.h
            kwin/kwinhybridtaskidentity.cpp
            kwin/kwinhybridpublicstate.cpp
            kwin/kwinhybridwindowactions.cpp
            kwin/kwinhybridwindowlifecycle.cpp
            kwin/kwinmemberpolicy.cpp
            kwin/kwinmemberpolicy.h
            kwin/kwintaskidentitymanager.cpp
            kwin/kwintaskidentitymanager.h
            kwin/kwintransientmanager.cpp
            kwin/kwintransientmanager.h
            kwin/hybridstackingorder.cpp
            kwin/hybridstackingorder.h
            kwin/kwinsceneadapter.cpp
            kwin/kwinsceneadapter.h
            kwin/layoutgeometry.cpp
            kwin/layoutgeometry.h
            kwin/main.cpp
            kwin/managedwindowregistry.cpp
            kwin/managedwindowregistry.h
            kwin/mutationcontrol.cpp
            kwin/mutationcontrol.h
            kwin/qindaqtkwinplugin.cpp
            kwin/qindaqtkwinplugin.h
            kwin/shellvisibilityrefreshscheduler.cpp
            kwin/shellvisibilityrefreshscheduler.h
            kwin/shellvisibilitywindowadmission.cpp
            kwin/shellvisibilitywindowadmission.h
        INSTALL_NAMESPACE "kwin/plugins"
    )
    target_link_libraries(
        qindaqt_compositor
        PRIVATE
            QindaQt::CompositorBridge
            QindaQt::Hybrid
            QindaQt::HybridChrome
            QindaQt::HybridConstraints
            QindaQt::HybridInput
            qindaqt_container_close_prompt
            qindaqt_group_context_menu
            qindaqt_kwin_development_input
            qindaqt_kwin_input_adapter
            qindaqt_kwin_output_support
            KDecoration3::KDecoration
            KWin::kwin
            KF6::GlobalAccel
            Plasma::Activities
            Qt6::DBus
            Qt6::Widgets
    )
    target_compile_definitions(
        qindaqt_compositor
        PRIVATE QINDAQT_KWIN_ABI_VERSION="${_qindaqt_kwin_abi_version}"
    )
    set_target_properties(
        qindaqt_compositor
        PROPERTIES
            CXX_EXTENSIONS OFF
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/kwin/plugins"
            # AGENT-NOTE: KWin scans its plugin namespace for module names,
            # not conventional Unix "lib"-prefixed shared libraries.
            PREFIX ""
    )
    qindaqt_enable_warnings(qindaqt_compositor)
endif()
