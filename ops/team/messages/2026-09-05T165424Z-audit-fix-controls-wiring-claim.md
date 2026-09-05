# Controls wiring claim

Base49210a4cae05344303144a21f693aff9248b1c53, branch codex/audit-controls-wiring.
Own shell/qml wiring, minimal shellruntimeapplication.h/.cpp composition hooks,
focused panel-geometry dispatcher tests and panel-surfaces wiring documentation.
No controller, manifest, registry, factory or CMake edits. Controls owner must add
new inert component inventory QML file to both module QML_FILES lists. Runtime
passes supplied PID directly; no additional saved options state. Pending final
per-applet facade properties and ApplicationTilesApplet from controls owner.
