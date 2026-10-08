# Archive2325 completed; observer failure preserved separately

One exact7791/18c6 archive attempt. ObserverFAILED with TimeoutExpired on a read-only systemctl show3s poll at38.932s; no automatic retry, no observer-held-reap claim. Saved actualmain2690287/start11860656 was D disk-sleep at one live observation; cannot attribute system-wide cause from this alone.

The original unit remained sameinvocation e62b11a46de746fa9b7574d54aa12223. Later authoritative journal: Started17:19:05, Deactivated successfully17:20:47,102.103swall/35.512sCPU/1.6Gpeak. Finalunitinactive/dead/MainPID0; bothknownmain2690287 and outer2690277/start11860642 absent. Originalarchive result success=true/101.920s, cpio/gzip/gzip-test all0; source/tool/base+clone inventory stable. Independently streamed newimagehash and checked7296oneguestobject/other7295parity. Laptopheavy RELEASED aftertheseobservations, not atobserverfailure.

Newroot2250/archive-2325/initramfs.cpio.gz262661435bytes SHAed599b4958f0eaabf53f4e8543e6030c105eade33e82944334b472e39fef105e root0400. Raw660558336bytes SHA4266d36d5e0daa03b23bb4b897405e92c68fadb6b95aa304581322f3e4337d07.
Proof .cache/android-console-archive-proof.tar.gz302359bytes SHAea0ec9d40585508dc9d0f483252acbe22dd4e8a123ecd18c8e0a87c373cd5127;21regularmembers20indexedpayloads independently rehashed qinda. Includes actualobserverfailure,livecaps,postfailureunit+launcherchecks,journal,actualarchive+parity. No existingrawrewritten.

## Exact tenth boot proposal, not executed

.cache/android-vm-tenth-20261008/plan.json SHA9bb5b46f84079e36ecc5908930a73095d0c3b2dc9d3fb8df4d1c656a87e71a32; manifestf02f96d7ea3e7601b1087988bdc3731122c6115260fc4ad7a0b0069671986286. Sameoriginalthreeimages/QEMU/source21reader; newfourthabove, gueste87f consoledeviceguard and preservedwindowscc4f. Same330/300/360s4CPU5GiB/no swap/0-7/nice10/tasks256. Observer retains existing3spoll; its timeout must never be treated asoriginalunittermination, as demonstrated here. No newlyauthoredsupervisionframework.

No tenthmanifest/guest launched. Requestrootreview of archiveevidence and separatebootgrant. Ninth/eighthFAILED and genuine separatetwowindowsobservations preserved; render/input/resizeclose/identity/Audio1remainopen.

### Exact next argv

```json
[
  "sudo",
  "-n",
  "systemd-run",
  "--unit=qindaqt-android-stock-tenth-20261008T2330Z.service",
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
  "/var/tmp/qindaqt-android-laptop-stage-20261008T2250Z/diagnostic-inputs-2325/tenth-manifest.json",
  "/var/tmp/qindaqt-android-laptop-stage-20261008T2250Z/vm-tenth-2330"
]
```
