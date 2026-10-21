# DualSense layout installed and verified

- Exact integrated implementation: `d4c85a4b5e5fd9c47ec1808b213c3549b9207e2c`.
- Overlay: `2dd46e984` on qinda master; source, candidate branches, archives and signed packages preserved on qinda.
- Paths: controller policy/runtime/input delivery/VoiceButton, Settings Input model/QML, focused controller tests, affected wiki/ADR and task/handoff records. No unrelated Desktop Harmony or animation source integrated.
- Build/gates: standalone controller build exit 0; six CTest groups exit 0, 43 QtTest results, zero failures/skips. Real QML section capability smoke passes. Signed native r38 and r39 builds exit 0; signature and embedded-recipe checks pass. r38 changes five Settings paths plus the plugin over r35, preserving 2,688 paths; r39 replaces only the plugin over r38, preserving 2,693 paths.
- Installed: Portage binary upgrade exit 0 to r39; installed Input/Controllers route exits 0 with ready witness. All 2,411 files/symlinks match; original KWin and shell processes remain alive. Loaded plugin is `qindaqt_controllers_r39`.
- Result: 19 exact bindings on one connected DualSense, three remembered DualSense devices and the family default. Non-binding motion prefs and other families unchanged; atomic file mode 0600. Private operational checkpoints live only in ignored build output.
- Boundaries: no physical button injection, microphone recording, or complete unrelated desktop suite; SDL/action-order/private-bus fixtures qualify the changed paths. No other model/worker launched. Shared default delivery remains the pre-existing baseline; r39 is the explicit laptop atom.
- Requested next action: use the installed layout; retain the integrated mapping and adopt r39 or its successor for later delivery. No implementation work remains for this outcome.
