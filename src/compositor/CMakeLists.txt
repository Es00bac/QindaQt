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
)

target_compile_features(qindaqt_compositor_bridge PUBLIC cxx_std_20)
target_link_libraries(qindaqt_compositor_bridge PUBLIC QindaQt::Core Qt6::Core)
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
    find_package(KWin ${_qindaqt_kwin_abi_version} EXACT REQUIRED CONFIG)

    add_library(qindaqt_kwin_input_adapter STATIC)
    target_sources(
        qindaqt_kwin_input_adapter
        PRIVATE
            kwin/inputcapabilities.cpp
            kwin/inputcapabilities.h
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
    target_link_libraries(qindaqt_kwin_input_adapter PUBLIC KWin::kwin Qt6::Core)
    set_target_properties(
        qindaqt_kwin_input_adapter
        PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
    )
    if(COMMAND qindaqt_enable_warnings)
        qindaqt_enable_warnings(qindaqt_kwin_input_adapter)
    endif()

    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins")
    kcoreaddons_add_plugin(
        qindaqt_compositor
        SOURCES
            kwin/kwincontrolendpoint.cpp
            kwin/kwincontrolendpoint.h
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
        INSTALL_NAMESPACE "kwin/plugins"
    )
    target_link_libraries(
        qindaqt_compositor
        PRIVATE
            QindaQt::CompositorBridge
            qindaqt_kwin_input_adapter
            KWin::kwin
            Qt6::DBus
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
