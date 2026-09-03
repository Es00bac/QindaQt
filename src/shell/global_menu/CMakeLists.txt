# SPDX-License-Identifier: LGPL-3.0-or-later

# AGENT-NOTE: G1 provides buildable production transports and a composition
# library, but deliberately does not instantiate them in qindaqt-shell. The
# applet registry, installed QML, submenu presentation, and runtime focus
# adapter remain owned by the later shell-composition lane. See
# docs/wiki/shell/global-menu.md and ADR-0056.
add_subdirectory(protocol)
add_subdirectory(ownership)
add_subdirectory(exporter)
add_subdirectory(qt_widgets_adapter)
add_subdirectory(applet)
add_subdirectory(registrar)
add_subdirectory(dbusmenu)
add_subdirectory(composition)
