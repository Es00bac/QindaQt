# Luna6 Notification Policy platform beep API correction

- 2026-09-24T00:06:27Z

The Program Manager approved binding sound requests to the Qt platform alert
output with `QGuiApplication::beep()`, but the installed Qt 6.11 headers do
not declare that method, and the production shell failed to compile. The
public static `QApplication::beep()` is the supported Qt entry point; the
installed implementation dispatches through Qt's GUI platform integration.
The shell already links Qt Widgets. I changed the binding to that callable
entry point without adding a dependency or changing the alert behavior, and
resumed the build.
