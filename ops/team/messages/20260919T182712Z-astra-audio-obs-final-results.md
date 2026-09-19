# Final audio/OBS checks and provisioning observations

## Source and compilation

Product candidate `3d44d74ef21136356af16989180d66d3041f2f41` was integrated by root before compilation; no product source changed during this final check phase. Exact bounded review of root's `c99d39c4` menu-name correction found the root references consistently renamed and the intentional `anchor.menu` property retained. Root reports desktop r8 and toolkit r2 installed on both hosts.

The worker compiled all eight audio/OBS test targets plus root's three UI targets in one Ninja invocation against main `build/dev`, exit 0. Actual `portageq envvar MAKEOPTS` was read as `-j24 -l24` and used unchanged; no build configuration or system limits were altered. Build log: main `.cache/ui-diagnostics-20260919/audio-obs-focused-build.log`.

## Exact focused test results

After root confirmed production compilation complete, seven rows passed in the initial eight-row run: `qindaqt.services-obs-client`, `qindaqt.services-obs-transport`, `qindaqt.settings-streaming-model`, `qindaqt.shell-obs-applet-presentation`, `qindaqt.shell-obs-applet-controller`, `qindaqt.settings-audio-model`, `qindaqt.settings-audio-page`. Log: main `.cache/ui-diagnostics-20260919/audio-obs-focused-ctest.log`.

The eighth row, `qindaqt.obs-bridge-libobs`, initially failed before bridge assertions: `obs_startup()` at fixture line77 could not open an X display, followed by the existing unconditional `obs_shutdown()` cleanup at line203 crashing. Interrupted attempts that produced no log were confirmed not started. An ordinary call with root's actual graphical display still could not open X. The final same-display call with external access passed 1/1, CTest exit0, 0.19s; no source change was made. Final log: main `.cache/ui-diagnostics-20260919/audio-obs-libobs-session-unsandboxed-ctest.log`. Seven passing rows were not repeated. All eight assigned rows therefore passed, with the display prerequisite supplied for the bridge row.

## Read-only provisioning observations

Both hosts' actual graphical shell buses were identified rather than assuming the user-manager bus. Each host has an OBS WebSocket JSON configuration present, server enabled, authentication required and port4455. Each actual graphical bus has a running Secret Service and exactly one unlocked scoped `application=qindaqt, service=obs-websocket` item, zero locked items. Only `NameHasOwner` and `SearchItems` were called; no credential was retrieved or printed, and no OBS state changed.

These satisfy the applet's provisioning-presence gates; no setup action is indicated. The client URL is the existing `ws://127.0.0.1:4455`; this observation does not assert the OBS server's bind address or that the stored password matches. Root owns actual authenticated connection observations. If that later rejects authentication, the existing public action is Settings → Streaming → Repair OBS setup, followed by restarting OBS; provisioning should not be overwritten merely to add the chip. Root has now placed OBS beside Audio in both saved top bars.

## Runtime dependencies and limits

No Settings1 schema or persisted preference keys changed, so no resident settings-service reload is required by this candidate. Reload the shell and Audio1 service, reopen Settings, and let OBS load the new plugin. Installed desktop r8's CONTENTS owns `/usr/lib64/obs-plugins/obs-qindaqt.so`; no separate installed OBS bridge package needs rebuilding. Root owns those runtime actions.

The existing rows do not qualify all new auth-rejected, paused/reconnecting, request-refusal, long-popup, held-fader and Audio1 owner-loss-preservation paths in a live session. Physical hardware, forced PipeWire endpoint destruction and raw lingering-sink recovery remain unqualified. ADR-0208's existing all-bus/all-raw-strip activation policy remains unchanged and can yield duplicate or pre-rack audio. No speculative product edits or new tests were introduced to expand this scope.
