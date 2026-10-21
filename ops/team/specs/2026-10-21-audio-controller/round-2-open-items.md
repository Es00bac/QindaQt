# Round 2: fix every open item (2026-10-21, owner: "Fix all")

All lanes branch from hub main `58f434b81`. No install or emerge; the package-once pass
follows when the owner is ready to log out. Independent review was waived by the owner.

| Lane | Branch | Worker | Scope |
| --- | --- | --- | --- |
| Audio meters | `fix/audio-meter-acquire-20261021` | Claude Opus | Audio1 `AcquireMeters`/`ReleaseMeters` (supersedes ADR-0394's always-on meters), meters on the worker's shared PipeWire core, passive meters so devices can suspend. |
| Audio endpoints | `fix/audio-endpoints-20261021` | Claude Opus | Recreate a virtual strip sink that PipeWire destroys without a reconnect; cache presets, macros and VBAN files instead of re-reading them per snapshot; fix the applet QML rows that flake under parallel ctest. |
| Wayland hang | `fix/wayland-dispatch-20261021` | Claude Sonnet | Settings "crashes" were KWin not-responding kills (SIGABRT from the fork's killer helper). Settings froze in `QtWaylandOutputManagementPort::dispatchReadable()` doing a blocking `wl_display_dispatch()` from a QSocketNotifier. The same pattern is in the clipboard Wayland adapter. Fix: the non-blocking prepare_read/read_events/dispatch_pending sequence. |
| PipeWire patch | overlay `pipewire-filter-graph-leak-20261021` | Claude Sonnet | Free the nodes and handles that PipeWire 1.6.8 filter-graph `impl_init` leaks on a failed load; carry it as an overlay revision; draft (do not submit) the upstream report. |

Evidence for the Settings freezes: laptop cores 732009, 799282, 616349 and 308786 all carry
siginfo `si_code=0` with a same-user sender. Core 799282's build-id matches installed r42;
its main thread is in `ppoll` under `wl_display_dispatch_queue` from the output-management
port's notifier slot.
