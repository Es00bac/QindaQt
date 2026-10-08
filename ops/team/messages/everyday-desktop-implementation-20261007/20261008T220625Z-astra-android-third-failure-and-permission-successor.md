# Third VM actual progress/failure and guest permission repair

Exact granted e0bc/c3d6 third VM settled1/10.877s. Held outer2540753/start11397724 reaped/absent; main2540758/start11397729, QEMU2540840/start11397948 retiredexit0/inputstable. Six actual4CPU5GiB/swap0/tasks256/nice10/halfcore0-7 witnesses. Actual stock init completed LXC7.0.0/configuration; guest later PermissionError before app windows. CleanupQualified=true is independently observed LXC STOPPED plus owned-child settlement, not inferred from QEMU containment. No app/identity success. Laptop heavy released.

Immutable proof .cache/android-third-vm-proof.tar.gz18494bytes SHAee5759a138d0ce5144aae631c92ace46ce0f90b2e2a16932d77481f6a9922f2e;8members/7raw indexedpayloads rehashed on qinda. Earlier failures/archives preserved.

Read-only full7208 inventory permission audit: usr/bin is the sole recorded directory lacking other traversal; all regularfiles other-readable. Host stage root itself0700 lies outside object records. Fixed executable/proof/scenario/plugin paths otherwise755/644. Guest /var is fresh755 tmpfs, but old main umask077 made generated /var/lib and stock runtime/config too restrictive. Existing /run/android-proof is guest-created; fixed busconfigs explicitly644, same private bus allows fixture UID1000. /run/user parents are explicitly755, runtime1000 is0700/chowned1000; Wayland socket lives there. /home/proof needs explicit0700 now conventional mask changes.

Root-authorized exact guest source00573bf502510a035f87173d87cbb46c0f918e82: guestPID1-only chmod / and /usr/bin0755; guest privatebus parent0755; conventional guest umask022 for stock runtime; explicit private HOME0700 plus existing runtime0700. Fixed error-stage enum, bounded errno and256char message in existingresult. No host stage modes/package bytes/isolation/success changes. PermissionError precise stage remains unobserved until next run; source evidence supports these concrete mismatches, not a claimed app fix.

Fresh archive2210 plan retains exact same7208 inventory and one guest.py delta, allother7207 unchanged. Driver/observer differ only approved guesthash/freshnames from accepted2200. Same120s4CPU5GiB/halfcore/nice10, no new supervisor. Source AST/diff checks only; archive/fourthVM UNRUN.

- guest.py: cba949adfe8361e6dc55e1c40e5c4489b03fe78cb523955f09d54d855eb1a9ea
- archive-delta.py: 55e1e1192cb245fd4dd6a4b4b806cf75c7b33ccfbb669afa9966351ec8cc49be
- plan.json: 81045b7b51e65ad389bfc99e3ba39b31be1b97cdc5f444ba810c5c4ef27b2d10
- run-once.py: 31319cbd7afc3329cd8e74f9b22aa7a60023359544edcb237a8feefc8b814f4a

~~~json
{
  "schema": 1,
  "source": "00573bf502510a035f87173d87cbb46c0f918e82",
  "executed": false,
  "root": "/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z",
  "freshOutput": "archive-2210",
  "protectedInputs": "diagnostic-inputs-2210",
  "guestSHA256": "cba949adfe8361e6dc55e1c40e5c4489b03fe78cb523955f09d54d855eb1a9ea",
  "scriptSHA256": "55e1e1192cb245fd4dd6a4b4b806cf75c7b33ccfbb669afa9966351ec8cc49be",
  "originalArchiveSHA256": "3542cc25515b1016e26f60f206f4f767eab54c764ab19dba11ba32b4fdb8ee46",
  "onlyChangedStageObject": "proof/guest.py",
  "stageObjectCount": 7208,
  "resourcePolicy": "same admitted120s/4CPU/5GiB/swap0/tasks256/nice10/affinity0-7 archive envelope; no guest",
  "argv": [
    "/usr/bin/python3",
    "-B",
    "/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z/diagnostic-inputs-2210/archive-delta.py"
  ],
  "unitArgv": [
    "sudo",
    "-n",
    "systemd-run",
    "--unit=qindaqt-android-diagnostic-archive-20261008T2210Z.service",
    "--wait",
    "--pipe",
    "--collect",
    "--expand-environment=no",
    "--working-directory=/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z",
    "--property=CPUQuota=400%",
    "--property=CPUAffinity=0-7",
    "--property=MemoryMax=5368709120",
    "--property=MemorySwapMax=0",
    "--property=TasksMax=256",
    "--property=Nice=10",
    "--property=RuntimeMaxSec=120",
    "--property=TimeoutStopSec=5",
    "--property=KillMode=control-group",
    "--property=LimitCORE=0",
    "--property=LimitFSIZE=1073741824",
    "--property=UMask=0077",
    "--property=UnsetEnvironment=PYTHONPATH PYTHONHOME LD_PRELOAD LD_LIBRARY_PATH DISPLAY WAYLAND_DISPLAY",
    "--setenv=PATH=/usr/bin:/bin",
    "--setenv=LANG=C.UTF-8",
    "--setenv=PYTHONDONTWRITEBYTECODE=1",
    "--setenv=DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent/qindaqt-android-stage-session",
    "--setenv=DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent/qindaqt-android-stage-system",
    "/usr/bin/python3",
    "-B",
    "/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z/diagnostic-inputs-2210/archive-delta.py"
  ],
  "staticNeeded": {
    "roots": 12,
    "resolvedObjects": 143,
    "missing": 0,
    "libgccSHA256": "720339a3fa2f886c782963762b15ab6a7bfb7859e947bc97d9d71fafc2f2de99",
    "owner": "sys-devel/gcc-15.3.0"
  }
}
~~~
