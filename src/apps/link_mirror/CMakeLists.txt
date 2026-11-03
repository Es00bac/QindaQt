# SPDX-License-Identifier: GPL-3.0-or-later
#
# QindaLink mirror windows (ADR-0433, docs/wiki/reference/qindalink-mirror-v1.md).
# qindaqt_link_mirror_core is pure: framing, message bodies, input mapping and the decoder
# selection policy, with no sockets or FFmpeg, so its tests run anywhere.

qt_add_library(qindaqt_link_mirror_core STATIC)
add_library(QindaQt::LinkMirrorCore ALIAS qindaqt_link_mirror_core)
target_sources(qindaqt_link_mirror_core
    PRIVATE
        src/mirror_protocol.cpp
        src/mirror_messages.cpp
        src/input_mapper.cpp
        src/key_mapping.cpp
        src/decoder_selection.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
        include/qindaqt/link_mirror/mirror_protocol.h
        include/qindaqt/link_mirror/mirror_messages.h
        include/qindaqt/link_mirror/input_mapper.h
        include/qindaqt/link_mirror/decoder_selection.h)
target_link_libraries(qindaqt_link_mirror_core PUBLIC Qt6::Core Qt6::Gui)
target_compile_features(qindaqt_link_mirror_core PUBLIC cxx_std_20)
set_target_properties(qindaqt_link_mirror_core PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_link_mirror_core)

if(NOT QINDAQT_BUILD_LINK_MIRROR)
    return()
endif()

# AGENT-CONTRACT: ADR-0435. libavcodec (VA-API through libavutil's hwcontext, software
# fallback) is the decoder; Portage provides it as media-video/ffmpeg[vaapi], and
# gui-wm/qindaqt-desktop must DEPEND on it or configure with -DQINDAQT_BUILD_LINK_MIRROR=OFF.
find_package(Qt6 6.11 REQUIRED COMPONENTS Network)
find_package(PkgConfig REQUIRED)
pkg_check_modules(QINDAQT_LINK_MIRROR_AV QUIET IMPORTED_TARGET libavcodec libavutil libswscale)
if(NOT QINDAQT_LINK_MIRROR_AV_FOUND)
    message(FATAL_ERROR
        "qindaqt-link-mirror needs FFmpeg (libavcodec, libavutil, libswscale; Portage "
        "media-video/ffmpeg[vaapi]), see docs/wiki/adr/0435-decode-qindalink-streams-with-ffmpeg.md. "
        "Install it, or configure with -DQINDAQT_BUILD_LINK_MIRROR=OFF to build without the mirror.")
endif()

qt_add_executable(qindaqt-link-mirror
    src/main.cpp
    src/mirror_options.cpp src/mirror_options.h
    src/adb_session.cpp src/adb_session.h
    src/mirror_connection.cpp src/mirror_connection.h
    src/ffmpeg_decoder.cpp src/ffmpeg_decoder.h
    src/decode_worker.cpp src/decode_worker.h
    src/mirror_window.cpp src/mirror_window.h
    src/mirror_controller.cpp src/mirror_controller.h
    src/evidence_runner.cpp src/evidence_runner.h)
target_link_libraries(qindaqt-link-mirror PRIVATE
    QindaQt::LinkMirrorCore PkgConfig::QINDAQT_LINK_MIRROR_AV Qt6::Gui Qt6::Network)
set_target_properties(qindaqt-link-mirror PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt-link-mirror)
install(TARGETS qindaqt-link-mirror RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT LinkMirror)
