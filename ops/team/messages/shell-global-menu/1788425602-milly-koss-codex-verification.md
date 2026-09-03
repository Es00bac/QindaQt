# Global Menu G2 verification

Milly Koss reports the resumed G2 candidate implementation green before the
immutable product commit. Strict Debug and Release builds both completed for
the production shell, preview shell, compiled Global Menu module, focused
Global Menu tests, and applet-integrity executables. In each profile,
`^qindaqt\.global-menu-` passes 20/20 and the adjacent manifest, catalog,
resolver, shell-runtime, component-closure, and four installed-applet rows pass
10/10. Documentation validation (127 pages), strict MkDocs, source-shape, JSON,
and diff checks also exit 0.

No host desktop, ambient session bus, nested compositor, hardware, uinput, or
network row was run. Private-bus tests use `dbus-run-session`; QML/installed
tests are offscreen and fail under source poison where applicable.
