# Codex controller integration

- Status: available — controller feature deployed on both hosts; requested laptop outcome complete
- Branch: feature/controller-settings
- Base: 43e822c77f9503dcb637008e9147ae384eb28e6e
- Worktree: isolated controller-settings worktree
- Ownership: src/controllers, Settings Input controller model and section, focused controller tests, ADR-0347 and controller wiki page; additive build and route registry changes

## Updates

- 2026-10-03T19:36:35Z — Claimed the user-requested controller feature. Implement in QindaQt over the installed compositor SDK; no KWin source rebuild. Batch one feature release after focused controller, Settings, and nested plugin checks. Steam and other game consumers retain priority. No physical controller is currently connected.
- 2026-10-03T20:00:00Z — Implemented controller profiles, consumer priority, SDL runtime, independent KWin plugin, Voice1 hold action, and Controllers Settings destination. Focused policy and Settings model targets compile; 2/2 suites pass in 0.03 seconds. SDL virtual runtime/plugin compile and UI qualification are next; no whole desktop build was started.

- 2026-10-03T20:16:00Z — Controller plugin compiled against installed compositor SDK. User explicitly stopped all further tests and audits; those gates are waived for this requested deployment. Product source is preserved on qinda at 05bd0a45776c34263c4e4a0d0c3f1a02aad4e205. Portage desktop r11 is compiling on qinda with BUILD_TESTING=OFF; laptop will consume its signed binary. SDL3 is already installed on both hosts through Portage. No desktop session restart is planned.

- 2026-10-03T20:30:50Z — Deployed gui-wm/qindaqt-desktop-0.1.0_pre20261002-r11 on qinda and qinda-top through Portage; both installation commands exited 0. Laptop consumed signed binaries for desktop r11 and KWin r5. Live LoadPlugin returned true; Controllers1 belongs to existing KWin PID 153380. Opened real Settings Input → Controllers as running PID 198725; graphical session 18 is active on VT 7. No further tests were run. Physical controller/game qualification remains unperformed under the user instruction; source candidate is preserved on qinda for manager integration.
