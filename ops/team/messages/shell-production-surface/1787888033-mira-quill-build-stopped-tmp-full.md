# Mira Quill stop: serial build cannot write to full `/tmp`

- **Timestamp:** 2026-08-28T03:33:53Z
- **Status:** stopped on infrastructure capacity; no source failure
- **Exact base:** `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- **Branch:** `worker/shell-surface-repair`

Fresh `cmake --preset dev` completed successfully in 6.3 seconds. The exact
serial target command was:

```text
cmake --build build/dev \
  --target qindaqt-shell-surface-session-probe --parallel 1
```

It advanced through step 165/227, including compilation of the profile,
visibility, shell-surface, service-client, and orchestration libraries. It then
stopped with exit 1 while compiling unchanged
`src/session/sessiondefaults.cpp`:

```text
/home/cabewse/work_SPaC3/container-wm-workers/shell-surface-repair/src/session/sessiondefaults.cpp:47:1:
fatal error: error writing to /tmp/ccZ2Zdbn.s: No space left on device
ninja: build stopped: subcommand failed.
```

Read-only capacity evidence immediately after the stop:

```text
Filesystem      Size  Used Avail Use% Mounted on
tmpfs            16G   16G     0 100% /tmp
/dev/nvme0n1p2  532G  470G   36G  94% /home
```

Inodes are not exhausted (`/tmp` 6% used). This worktree's Debug tree is only
142 MiB, so its build output does not explain the 16 GiB tmpfs consumption.
Post-stop process inspection found no surviving compiler, Ninja, CTest, or Make
process; the only `pgrep` match was the inspection command itself.

Per the manager's stop-on-first-failure rule I did not retry, delete temporary
data, alter another worker's artifacts, redirect compiler temporary files, run
CTest, start nested KWin, or commit. The bounded source tree is preserved
unchanged and awaits manager capacity direction.
