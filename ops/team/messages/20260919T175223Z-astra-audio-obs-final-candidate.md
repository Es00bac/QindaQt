# Final audio/OBS candidate and bounded review

- Product-code candidate: `3d44d74ef21136356af16989180d66d3041f2f41`.
- Integrate its history from exact base `01e919f6bff7c3c111778f814aaec8288ddd7777`: diagnostics `433069bd`, corrections `74553748`, source-preservation repair `3d44d74e`.
- Branch/worktree: `agent/audio-obs-20260919`, `.cache/audio-obs-20260919`.
- Root reviewed and accepted the final source repair. All product source is frozen; this reply adds documentation only.
- `git diff --check` passed. No compilation, configure check, runtime test, new test, live session mutation or installation was performed by this worker.

## Final correction

Audio1 owner loss and collection loading without an authoritative snapshot now silence only the capture kind/device settings on owned OBS sources. Console identities, names, scene/filter/mixer associations and other settings survive; the next authoritative snapshot retargets those same sources. The mapping and dock still clear while authority is absent. AudioSettingsModel's availability comment now explicitly distinguishes snapshot capability from per-target pending guards and queued Busy admission.

## UI source review

Bounded read-only review of desktop `ecb67171` and toolkit `d59b080` found no clear QML compile/runtime blocker. Reviewed Dialog's content alias and nonvisual Connections ownership, Scroll's single visual child sizing, menu popup overload against installed Qt 6.11.1 headers, overflow wrapper depth and anchor, full sidebar columns/focus reveal, preference page implicit sizing and the Customize nested-scroll removal. This is source review, not runtime qualification.

## Existing focused checks after compilation

The smallest useful existing rows covering the changed boundaries are `qindaqt.services-obs-client`, `qindaqt.services-obs-transport`, `qindaqt.shell-obs-applet-controller`, `qindaqt.shell-obs-applet-presentation`, `qindaqt.settings-streaming-model`, `qindaqt.settings-audio-model`, `qindaqt.settings-audio-page` and `qindaqt.obs-bridge-libobs`. These have not run. Existing tests do not prove the newly corrected owner-loss preservation or all paused/reconnecting/auth-rejected paths; root's focused final session check must cover those, a held console fader through snapshots, refused-operation feedback and the long OBS popup.

## Saved top-bar placement

Read-only local and SSH checks show both qinda and qinda-top select `qindaqt`, and neither saved `~/.local/share/qindaqt/profiles/qindaqt.json` has an OBS applet on its top `command-bar`. The repository default already places one. Root owns actual placement after installation.

Public UI: Meta+right-click panel → Add applet → OBS. The shell's existing invokable is `LiveCustomizationController.addApplet("command-bar", "end", "obs")`: a free instance id and InsertAppletIntent flow through `settle(..., true)` → `LiveEditorHost.apply()` → `EditorSession.applyToUserProfile()`/`UserProfileStore`, and the shell's store watcher adopts the saved layout. The generic menu chooses the first admitted zone, so its newly added chip may need moving to End through its applet menu.

## Unchanged limitations

ADR-0208 still activates every bus and raw strip in OBS; duplicate strip/bus paths and raw pre-rack captures remain explicit policy limitations. Normal console reprojection preserves delegates, but arbitrary same-count identity replacement remains unqualified. Raw lingering-sink proxy recovery and physical hardware behavior were not validated. Build policy remains unchanged: root read qinda `-j24 -l24`, qinda-top `-j16 -l16`; Portage inherits configuration, and any direct build must match its host's actual limits.
