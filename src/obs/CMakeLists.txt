# SPDX-License-Identifier: GPL-3.0-or-later
#
# The OBS console bridge (ADR-0208): a libobs module built against the
# installed OBS. This directory configures alone (the media-plugins ebuild
# builds only it); repository builds add it from src/CMakeLists.txt and skip
# it quietly when the OBS development files are absent.

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.25)
    project(QindaQtObsBridge LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    include(GNUInstallDirs)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
    include(QindaQtCompiler)
    option(QINDAQT_ENABLE_STRICT_WARNINGS "Treat QindaQt warnings as errors" ON)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus Widgets)
    qt_standard_project_setup(REQUIRES 6.11)
    # AGENT-GUARD: set BEFORE the add_subdirectory calls, not after. The audio
    # protocol and client install their targets and headers unconditionally,
    # and media-plugins/obs-qindaqt owns only the libobs module - so a
    # standalone build that installed them collided with gui-wm/qindaqt-desktop
    # on ten files (/usr/lib64/libqindaqt_audio_{client,protocol}.a and eight
    # headers) and Portage refused the merge. The subdirectories read this
    # variable to suppress their own install rules; it is only visible to them
    # if it is set first.
    set(QINDAQT_OBS_BRIDGE_STANDALONE ON)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../services/audio_protocol"
                     "${CMAKE_CURRENT_BINARY_DIR}/audio_protocol")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../services/audio_client"
                     "${CMAKE_CURRENT_BINARY_DIR}/audio_client")
endif()

find_package(libobs QUIET)
find_package(obs-frontend-api QUIET)
find_package(obs-websocket-api QUIET)
if(NOT libobs_FOUND OR NOT obs-frontend-api_FOUND)
    if(QINDAQT_OBS_BRIDGE_STANDALONE)
        message(FATAL_ERROR "obs-qindaqt needs libobs and obs-frontend-api development files")
    endif()
    message(STATUS "QindaQt OBS bridge: libobs or obs-frontend-api not found; obs-qindaqt is skipped")
    return()
endif()

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus Widgets)

# The module is a shared object, so the static audio libraries it links must
# be position independent; until this module they were only ever linked into
# executables. Setting the property here keeps that need with its consumer.
set_target_properties(
    qindaqt_audio_protocol qindaqt_audio_client
    PROPERTIES POSITION_INDEPENDENT_CODE ON
)

# The pure half: the console-to-source projection and the sync plan.
qt_add_library(qindaqt_obs_bridge_core STATIC)
add_library(QindaQt::ObsBridgeCore ALIAS qindaqt_obs_bridge_core)
target_sources(
    qindaqt_obs_bridge_core
    PRIVATE core/src/console_sources.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS core/include
        FILES core/include/qindaqt/obs_bridge/console_sources.h
)
target_link_libraries(qindaqt_obs_bridge_core PUBLIC QindaQt::AudioProtocol Qt6::Core)
target_compile_features(qindaqt_obs_bridge_core PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_obs_bridge_core
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_obs_bridge_core)

# The libobs half, as a static library so the headless rows can drive the
# sync in-process; the module below is that library plus the entry points.
qt_add_library(qindaqt_obs_bridge_module STATIC)
add_library(QindaQt::ObsBridgeModule ALIAS qindaqt_obs_bridge_module)
target_sources(
    qindaqt_obs_bridge_module
    PRIVATE
        module/bridge_controller.cpp
        module/bridge_controller.h
        module/console_dock.cpp
        module/console_dock.h
        module/console_source_types.cpp
        module/console_source_types.h
        module/websocket_vendor.cpp
        module/websocket_vendor.h
)
target_include_directories(qindaqt_obs_bridge_module PUBLIC module)
target_link_libraries(
    qindaqt_obs_bridge_module
    PUBLIC
        QindaQt::ObsBridgeCore
        QindaQt::AudioClient
        Qt6::Core
        Qt6::DBus
        Qt6::Widgets
        OBS::libobs
        OBS::obs-frontend-api
)
if(obs-websocket-api_FOUND)
    target_link_libraries(qindaqt_obs_bridge_module PUBLIC OBS::obs-websocket-api)
    target_compile_definitions(qindaqt_obs_bridge_module PUBLIC QINDAQT_OBS_WEBSOCKET_API=1)
endif()
target_compile_features(qindaqt_obs_bridge_module PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_obs_bridge_module
    PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_obs_bridge_module)

add_library(obs-qindaqt MODULE module/obs_qindaqt_module.cpp)
target_link_libraries(obs-qindaqt PRIVATE QindaQt::ObsBridgeModule)
set_target_properties(obs-qindaqt PROPERTIES PREFIX "" CXX_EXTENSIONS OFF)
target_compile_features(obs-qindaqt PRIVATE cxx_std_20)
qindaqt_enable_warnings(obs-qindaqt)

# OBS loads modules from <libdir>/obs-plugins; the module carries no locale
# or data files.
install(
    TARGETS obs-qindaqt
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/obs-plugins"
    COMPONENT ObsBridge
)
