# Readiness archive2305 passed; eighth VM unexecuted

Exact corrected981d/3f65 grant executed once: cpio/gzip/gzip-test all0. Full original and cloned inventory/pins stable,7296objects, only proof/windows.py changed;7295otherobjects unchanged. Rejected wrong-cpio plan remains preserved/unexecuted.

Observer0/22.178s; held outer2644914/start11720228 reaped/procabsent. Main2644922/invocation03ed8e813dc641e1b878c2d80ff71fc4; six live cap witnesses4CPU5GiB/no swap/tasks256/0-7/nice10/120s. Finalunitinactive/dead/MainPID0/resultsuccess; laptopheavy RELEASED.
New protected root2250/archive-2305/initramfs.cpio.gz262661670bytes SHA256af9cc29e042a36b783813c2ddbc52ee21a4b1d9882255c69ea795a1e6df96efb, root0400. Raw660556800bytes SHA710e3a4c969f2a0a62329d0fe26cd413ecf1659418a113777bd75e4031893139.

Proof .cache/android-readiness-archive-proof.tar.gz298541bytes SHAb77b9e0357cc8ef8f51b10dec3f805b5ca8bf73a25fb9d7115ba621a43005229;17regularmembers16indexedpayloads independently rehashed on qinda.

## Eighth VM proposal

.cache/android-vm-eighth-20261008/plan.json SHA3063b89574f430675bc78e7097a2d0e6759d3d7005b9f3c594d0e6c442801dca; manifest272ba5fc5ddc5b221a169d6b26bb6121bfc406415e8fff527d319c2bc4c2a3dd. Same originalthree stockinputs, newfourthabove, unchangedprotected21source/QEMU/reader. Only archivedwindows.pycc4f readinessdelta. Same330unit/300inner/360observer4CPU5GiB/no swap/0-7/nice10/tasks256.

No protected eighthmanifest or guest launched. Request rootreview and one grant. SeventhactualprivatePulse/container/baseline qualification retained; bootcompleted/apps/render/input/resize/close and Audio1integration remain open. Newdiagnostics retain bounded timeout output and actualstage; no timeout retry.

### Exact next argv

```json
[
  "sudo",
  "-n",
  "systemd-run",
  "--unit=qindaqt-android-stock-eighth-20261008T2310Z.service",
  "--wait",
  "--pipe",
  "--collect",
  "--expand-environment=no",
  "--working-directory=/var/tmp/qindaqt-android-laptop-stage-20261008T2250Z",
  "--property=CPUQuota=400%",
  "--property=CPUAffinity=0-7",
  "--property=MemoryMax=5368709120",
  "--property=MemorySwapMax=0",
  "--property=TasksMax=256",
  "--property=Nice=10",
  "--property=RuntimeMaxSec=330",
  "--property=TimeoutStopSec=5",
  "--property=KillMode=control-group",
  "--property=LimitCORE=0",
  "--property=LimitFSIZE=67108864",
  "--property=UMask=0077",
  "--property=UnsetEnvironment=PYTHONPATH PYTHONHOME LD_PRELOAD LD_LIBRARY_PATH DISPLAY WAYLAND_DISPLAY",
  "--setenv=PATH=/usr/bin:/bin",
  "--setenv=LANG=C",
  "--setenv=PYTHONDONTWRITEBYTECODE=1",
  "--setenv=DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent/android-vm-session",
  "--setenv=DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent/android-vm-system",
  "/usr/bin/python3",
  "-B",
  "/var/tmp/qindaqt-android-laptop-stage-20261008T2250Z/inputs/source/run_vm.py",
  "--execute-manager-admitted",
  "/var/tmp/qindaqt-android-laptop-stage-20261008T2250Z/diagnostic-inputs-2305/eighth-manifest.json",
  "/var/tmp/qindaqt-android-laptop-stage-20261008T2250Z/vm-eighth-2310"
]
```
