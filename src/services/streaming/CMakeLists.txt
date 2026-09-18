# SPDX-License-Identifier: GPL-3.0-or-later

# The virtual camera's kernel-module configuration. These two files have
# shipped in the tree since the streaming work began but nothing installed
# them, which is why "Start Virtual Camera" failed on a fresh machine with no
# usable diagnosis: there was no loopback device for OBS to write into.
#
# AGENT-GUARD: modprobe and systemd-modules-load read `lib/modprobe.d` and
# `lib/modules-load.d` under the prefix, never `lib64`, even on a multilib
# system. Using CMAKE_INSTALL_LIBDIR here would install into a directory
# nothing reads (verified against kmod and systemd-modules-load on the
# desktop, 2026-09-17).
set(QINDAQT_MODPROBE_DIR "lib/modprobe.d")
set(QINDAQT_MODULES_LOAD_DIR "lib/modules-load.d")

install(
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/data/modprobe.d/qindaqt-virtual-camera.conf"
    DESTINATION "${QINDAQT_MODPROBE_DIR}"
    COMPONENT StreamingRuntime
)
install(
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/data/modules-load.d/qindaqt-virtual-camera.conf"
    DESTINATION "${QINDAQT_MODULES_LOAD_DIR}"
    COMPONENT StreamingRuntime
)
