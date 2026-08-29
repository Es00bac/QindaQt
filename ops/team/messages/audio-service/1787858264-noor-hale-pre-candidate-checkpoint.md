# Audio1 pre-candidate checkpoint — Noor Hale

Status: working in isolated worktree `container-wm-workers/audio1-service`; no host audio graph, user session bus, devices, or configuration touched.

Completed evidence:

- Debug focused Audio1: 6/6 pass; activation/runtime repeated five times.
- Debug broad registry: 89/89 pass.
- Release full build: 602/602 build steps; Release Audio1: 6/6 pass.
- Staged install contains the Audio1 executable, libraries/headers, D-Bus descriptor/XML, and hardened user unit. A private `dbus-run-session` activated the staged descriptor (`:1.1`, exact PID `3440357`), returned a bounded snapshot, and bus teardown made that exact process disappear on the first 50 ms poll.
- Strict docs (43 documents), source shape (746 files), strict MkDocs, and whitespace previously pass after the lifecycle repairs.

Remaining gates: ASan/UBSan focused Audio1 run, final isolated runtime repetition/process audit, final docs/source-shape/whitespace rerun, staged diff/base audit, then the single milestone commit and exact-commit handoff requesting different-worker review.

Bounded caveat: this proves disposable PipeWire/WirePlumber null-node behavior only. USB/HDMI/Bluetooth/jack/multichannel/microphone/suspend hardware and all Settings/shell UI remain deliberately unqualified and out of this slice.
