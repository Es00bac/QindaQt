# Rhea Calder — virtual desktop private-boot material FAIL

- Timestamp: 2026-08-28T12:37:24Z
- Accepted HEAD at reproduction: `dc377388af530411c3c281cb0171ccfc74590b0e`
- Live row: `desktop.virtual.boot.1080p`
- CTest result: fixture PASS, boot FAIL, exit 8
- Fresh run ID: `dd921db781ad35fc53d846999d32fc04`

The exact manager-acknowledged row failed closed in 0.11 seconds before Python,
KWin, the private bus, session, shell, services, test applications, or probe
could start:

```text
bwrap: execvp /home/linuxbrew/.linuxbrew/Cellar/python@3.14/3.14.2_1/bin/python3.14: No such file or directory
```

The fresh authenticated result archives `result.json`, `sandbox.log`, and the
complete `sandbox-command.json`. It records outcome `failure`, return code 1,
no timeout, no forbidden environment, and false host-input/runtime/render
bindings. The private run root is absent and no owned process survives; the
pre-existing host KWin/session and unrelated Flatpak processes are unchanged.
Screenshot count is zero both because execution never reached a desktop and
because S0+S1 makes no screenshot claim.

The command evidence makes the cause exact: CMake discovered a Homebrew Python
whose executable and libraries are covered by the read-only `.linuxbrew`
mount, but the ELF interpreter follows `.linuxbrew/lib/ld.so` to absolute
`/lib64/ld-linux-x86-64.so.2`. The empty bubblewrap root mounts `/usr` but does
not construct the host's merged-usr `/lib64 -> /usr/lib` alias. This is a
candidate containment-construction defect, not a host/tool availability issue.

The task permits a separate descendant when the private row reproduces a
failure. Rhea is retaining exclusive lane ownership and will make only the
owned sandbox/hostile-unit/ADR-testing-authority repair needed to construct the
authenticated merged-usr library aliases inside the sandbox. Source-safe gates
and the same exact live command will then be rerun. No host endpoint or broader
mount will be added.
