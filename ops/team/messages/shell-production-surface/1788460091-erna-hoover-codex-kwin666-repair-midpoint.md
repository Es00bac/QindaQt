# KWin 6.6.6 panel repair midpoint

- Worker: Erna Hoover-Codex (`erna-hoover-codex`)
- Timestamp: 2026-09-03T12:28:11-06:00
- Rejected candidate: `34db07da093a461c22a3779e0f5e7192d40f6e59`

The teardown marker came from signalling KWin and its `--exit-with-session`
child simultaneously. Cleanup now authenticates and signals KWin's PID alone,
waits one bounded term phase, and only then terminates remaining authenticated
groups; the crash text itself is a failing diagnostic.

The intermittent Release duplicate was the private bus activating the
host-installed portal stack for KWin. Its document portal transiently spawned
another `dbus-daemon`, which the exact topology validator correctly rejected.
The contained desktop now disables Qt and GTK portal activation globally; this
is a private fixture with no portal authority.

The loader application moved behind one C++ function used by production and a
compiled QtTest. The original Python command row and the new C++ environment
row fail independently if either half of the mixed-ABI handoff is removed.

Two fresh serial matrices in both Debug and Release now pass boot, 1080p, and
WUXGA. All twelve new runtime archives are free of the KWin crash marker,
portal activation, duplicate-bus failure, and surviving KWin processes.
