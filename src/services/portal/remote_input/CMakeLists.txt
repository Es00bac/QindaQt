# SPDX-License-Identifier: GPL-3.0-or-later
# Native RemoteDesktop backend over the selected compositor's EIS transport.
# portal_library() comes from ../foundation; adapted upstream sources keep
# their LGPL-2.0-or-later notices (docs/wiki/adr/0335-native-remote-input-portal.md).
portal_library(qindaqt_portal_remote_input PortalRemoteInput)
target_sources(qindaqt_portal_remote_input PRIVATE src/compositor_eis.cpp src/remote_sessions.cpp
    src/remote_sessions_p.h src/remote_desktop_adaptor.cpp src/input_capture_adaptor.cpp
    src/clipboard_adaptor_p.h src/clipboard_adaptor.cpp src/legacy_input_p.h src/legacy_input.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
    include/qindaqt/services/portal/remote_input/compositor_eis.h
    include/qindaqt/services/portal/remote_input/input_capture_adaptor.h
    include/qindaqt/services/portal/remote_input/remote_desktop_adaptor.h)
# PortalCapture supplies the CaptureUI port and ScreenCast source seam for
# combined RemoteDesktop+ScreenCast sessions; capture never depends back on us.
target_link_libraries(qindaqt_portal_remote_input PUBLIC QindaQt::PortalAccessPolicy QindaQt::PortalCapture Qt6::DBus)
# libei (dev-libs/libei, already required by the compositor fork) is the
# sender side for deprecated Notify* calls over the session's EIS context.
find_package(PkgConfig REQUIRED)
pkg_check_modules(QINDAQT_LIBEI REQUIRED IMPORTED_TARGET libei-1.0)
target_link_libraries(qindaqt_portal_remote_input PRIVATE PkgConfig::QINDAQT_LIBEI)
