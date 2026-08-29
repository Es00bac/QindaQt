# Mira Quill result: target build passes; nested runtime path is too long

- **Timestamp:** 2026-08-28T03:37:27Z
- **Status:** stopped after private-runtime infrastructure failure
- **Exact base:** `94e84077e33a279dcebee24511e7dbdf1b87e3e1`

The resumed serial target build passed **63/63 remaining steps, exit 0** under
the ignored worktree-local compiler temporary directory. Together with the
pre-capacity segment, the exact production shell, launcher, and modified
surface probe built successfully with strict warnings.

I then ran the exact three-row selector with `--parallel 1` and the manager-
directed TMPDIR/TMP/TEMP. CTest completed the selector as one command and
reported **0/3, exit 8**, but no row reached the candidate behavior. Every row
failed at nested KWin socket creation because Python's temporary root inherited
the long compiler path:

```text
error: socket path
"/home/cabewse/work_SPaC3/container-wm-workers/shell-surface-repair/
build/dev/compiler-tmp/qindaqt-shell-surface-<random>/runtime/qindaqt-0"
plus null terminator exceeds 108 bytes
FATAL ERROR: could not add wayland socket qindaqt-0
```

Exact rows recorded in `build/dev/Testing/Temporary/LastTestsFailed.log`:

```text
107:shell.production-surface.1080p
108:shell.production-surface.wuxga
109:shell.production-surface.1440p
```

The failures took 0.50, 0.25, and 0.23 seconds. No surface probe result was
emitted and no product protocol assertion ran. Post-test inspection found no
surviving process whose command line references this worktree, the temporary
fixture root, or CTest; the sole match was the inspection command itself.

I stopped without retry, source change, symlink, deletion, use of `/tmp`, or
commit. The next safe execution needs a manager-approved short absolute
temporary-path alias backed by the same ignored worktree directory, or an
equivalent short path on the non-full `/home` filesystem; Wayland's 108-byte
socket boundary makes the long canonical worktree path unusable for nested
runtime files.
