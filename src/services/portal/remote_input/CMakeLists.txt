# SPDX-License-Identifier: GPL-3.0-or-later
# Native RemoteDesktop backend over the selected compositor's EIS transport.
# portal_library() comes from ../foundation; adapted upstream sources keep
# their LGPL-2.0-or-later notices (docs/wiki/adr/0335-native-remote-input-portal.md).
portal_library(qindaqt_portal_remote_input PortalRemoteInput)
target_sources(qindaqt_portal_remote_input PRIVATE src/compositor_eis.cpp src/remote_sessions.cpp
    src/remote_sessions_p.h src/remote_desktop_adaptor.cpp src/input_capture_adaptor.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
    include/qindaqt/services/portal/remote_input/compositor_eis.h
    include/qindaqt/services/portal/remote_input/input_capture_adaptor.h
    include/qindaqt/services/portal/remote_input/remote_desktop_adaptor.h)
target_link_libraries(qindaqt_portal_remote_input PUBLIC QindaQt::PortalAccessPolicy Qt6::DBus)
