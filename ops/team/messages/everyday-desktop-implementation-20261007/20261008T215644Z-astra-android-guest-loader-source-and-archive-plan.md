# Qualified guest loader directory — source and next archive plan

Exact source bc0e108bd8b76be9ecf337984d4745f915e3f16e adds only fixed guest LD_LIBRARY_PATH=/usr/lib/gcc/x86_64-pc-linux-gnu/15 after the existing complete environment clear. User-session env derives from the same fixed guest dictionary. No host cache/config, package bytes, success predicate, isolation or runtime bounds change. This reproduces normal Gentoo loader lookup for an already admitted GCC runtime; it is not a new package dependency.

Read-only static check parsed DT_NEEDED/RPATH/RUNPATH with installed readelf against guest-rooted symlinks/default directories plus that fixed path. Twelve directly reached native roots (LXCinfo/start/stop, Python, D-Bus, QindaQtWM/KWin, mount/modprobe/Bash/nft/dnsmasq) resolve143 ELF objects, zero missing. No staged binary executed, no host loader cache consulted. This is static linkage evidence, not plugin/dlopen/Python/GI or guest compatibility. Qualified libgcc_s.so.1 is182360bytes SHA256720339a3fa2f886c782963762b15ab6a7bfb7859e947bc97d9d71fafc2f2de99, exact original inventory owner sys-devel/gcc-15.3.0.

Proposed fresh archive2200 uses the exact accepted2150 driver with only guest pin/fresh names changed. Same original7208 inventory, exactly one guest.py record delta, allother7207 fixed. Original stage/archives/first two failures retained. Same120s4CPU5GiB/halfcore0-7/nice10 and existing held observer; no archive or third VM executed. Root exact source/argv admission and per-host lease required.

- guest.py: 98860a07b2b982a27068fa9ffe0b445ab97e95f3fe12d5738b6f2395a500207d
- archive-delta.py: ee48e763173d9169f6b1fdd55cc4c9df1a17e45ac3902fb03bff3898b6ac2a0a
- plan.json: d49400e5e8f9d9b7b31a011107892b069bc86a98125760f1d7387d2e7639b998
- run-once.py: a2df090cc6b7fb0cd2e4dd2435c4beaf4f8fe030abd522a371ed9ec92d40da27
- check-needed.py: 6e1eab04e8184c07b9e7873b38fbc8f6967876c066055f1039feeffb3f53930b
- needed-result.json: 75842d299c180d21345c0d21e135b3813bde14306bdafc64d884398da6ce3c81

## Exact plan
~~~json
{
  "schema": 1,
  "source": "bc0e108bd8b76be9ecf337984d4745f915e3f16e",
  "executed": false,
  "root": "/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z",
  "freshOutput": "archive-2200",
  "protectedInputs": "diagnostic-inputs-2200",
  "guestSHA256": "98860a07b2b982a27068fa9ffe0b445ab97e95f3fe12d5738b6f2395a500207d",
  "scriptSHA256": "ee48e763173d9169f6b1fdd55cc4c9df1a17e45ac3902fb03bff3898b6ac2a0a",
  "originalArchiveSHA256": "3542cc25515b1016e26f60f206f4f767eab54c764ab19dba11ba32b4fdb8ee46",
  "onlyChangedStageObject": "proof/guest.py",
  "stageObjectCount": 7208,
  "resourcePolicy": "same admitted120s/4CPU/5GiB/swap0/tasks256/nice10/affinity0-7 archive envelope; no guest",
  "argv": [
    "/usr/bin/python3",
    "-B",
    "/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z/diagnostic-inputs-2200/archive-delta.py"
  ],
  "unitArgv": [
    "sudo",
    "-n",
    "systemd-run",
    "--unit=qindaqt-android-diagnostic-archive-20261008T2200Z.service",
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
    "/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z/diagnostic-inputs-2200/archive-delta.py"
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
