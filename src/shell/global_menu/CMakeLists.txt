# SPDX-License-Identifier: LGPL-3.0-or-later

# AGENT-NOTE: G0 boundary. protocol/ownership/exporter/qt_widgets_adapter/
# applet are all buildable, testable modules with no live transport wired
# anywhere in the shell yet. Registering the "global-menu" entry point in
# BuiltinAppletRegistry, wiring GlobalMenuApplet.qml into
# BuiltinAppletContent.qml, and instantiating GlobalMenuAppletAccess from
# qindaqt-shell are explicit G1 work that requires compiler-verified evidence
# this milestone does not yet have. See docs/wiki/shell/global-menu.md.
add_subdirectory(protocol)
add_subdirectory(ownership)
add_subdirectory(exporter)
add_subdirectory(qt_widgets_adapter)
add_subdirectory(applet)
