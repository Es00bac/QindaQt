# Rhea Calder — virtual desktop private-boot second material FAIL

- Timestamp: 2026-08-28T12:41:05Z
- Repair descendant: `e2ab439c79277464ebd9a9a8cba7d44b502cf17e`
- Tree: `94179e0c18d1c53cb0a6b8619491487f8e8df9e8`
- Parent: accepted candidate `dc377388af530411c3c281cb0171ccfc74590b0e`
- Fresh run ID: `ed904616f1e8ae03339c338ad846f1bf`

The first descendant's exact safe evidence passes 16/16 focused
sandbox/result units, 43/43 full desktop-session units, Python compilation,
64-document validation, 993-file source shape, whitespace, and the two safe
CTest rows. The same acknowledged live row proves the merged-usr loader repair:
Python entered the private namespace and started the early topology. It then
failed closed before the private Wayland socket because required processes 4
and 5 exited.

The fresh authenticated archive contains the command/result metadata and all
four logs created before failure: dbus-daemon, Settings1, Audio1, and
compositor. The exact causal log is:

```text
dbus-daemon: Could not get password database information for UID 1000:
Looking up user ID 1000: No such file or directory
```

The private bus therefore exits and Settings1/Audio1 follow. No KWin socket,
session, shell, test application, probe, UI, input, screenshot, or host endpoint
was reached. The private run root is absent and no owned process survives.

The next repair remains inside the existing sandbox boundary: create fresh
per-run synthetic passwd/group records for only the current mapped UID/GID,
authenticate them with the run sentinel/identity, and read-only bind them as
the empty root's `/etc/passwd` and `/etc/group`. Host account databases will
not be mounted. Focused hostile tamper tests, ADR/testing documentation, the
safe matrix, and the same exact live row will follow while Rhea retains the
exclusive lane.
