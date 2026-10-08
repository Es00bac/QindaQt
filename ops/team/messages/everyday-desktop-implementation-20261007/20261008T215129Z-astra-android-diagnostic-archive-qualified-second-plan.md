# Diagnostic archive2150 qualified; second VM plan awaits grant

Actual once-only archive2150 succeeds: outer0/21.772s, held2523619/start11323048 reaped/proc absent. Six actual live kernel/unit witnesses CPU400000/100000, memory5368709120, swap0, tasks256, affinity0-7, nice10. Final unit inactive/dead/MainPID0. Cpio0/2.720s, gzip0/6.879s, gzip-test0/2.269s; all stderr empty. No guest ran in this phase. Laptop heavy RELEASED.

Before/after original inventory7208 and accepted20 source/tool pins stable. Modified inventory7208 stable, independently compared: only proof/guest.py size/hash changed, same mode; other7207 records equal. Original initramfs3542cc streaming hash reconfirmed unchanged. New root0400 archive260225054bytes SHA25610594503ee7edf38d8daf0284f3439402ec78b16140e017c569952248ee111db; rawcpio654588928 SHA1f13ac0d6a8bccb0c1eb4dcf417b4fbe3058ffc05efa4241006f0b3deff1fb66.

Qinda ignored proof .cache/android-parent-diagnostic-archive-20261008/proof.tar.gz:333994bytes SHA2569e746f803426c0004fb66b8b2d65ecd008cb0ca4ae081424310ffb66b2a16e79,13regularmembers/12payloads rehashed independently on qinda. Actual result, logs, caps, full modified inventory/plan and exact scripts/argv preserved.

Prospective second plan SHA256dcd4ba188d2908380ff47ddabe50a493198b444c348b828c1a195dc15796a86b; manifest9b583fc2fa79a054ed3a2e8124da2eb5c345eb9466ba2978c58268c459d3e790. Three system/vendor/kernel inputs unchanged. Only initramfs fourth pin/path changes, fresh second unit/output, new protected manifest. Existing runtime reader20sourcepins and330/300/360s4CPU5GiB bounds unchanged. Guest beb2 only mkdir guest /var/lib and stock details-to-stdout. First init failure and unqualified inner cleanup remain. No second VM grant/execution implied; request one bounded diagnostic boot after exact new-input review.

## Exact next plan
~~~json
{
  "schema": 1,
  "kind": "prospective-second-stock-android-kvm-diagnostic-dispatch",
  "executed": false,
  "host": "qinda-top",
  "unit": "qindaqt-android-stock-second-20261008T2155Z.service",
  "argv": [
    "sudo",
    "-n",
    "systemd-run",
    "--unit=qindaqt-android-stock-second-20261008T2155Z.service",
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
    "/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z/inputs/source/run_vm.py",
    "--execute-manager-admitted",
    "/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z/diagnostic-inputs-2150/second-manifest.json",
    "/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z/vm-second-2155"
  ],
  "manifest": {
    "schema": 1,
    "resourceProfile": "laptop",
    "inputs": {
      "system": {
        "path": "/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z/vm-inputs/system",
        "size": 1827717120,
        "sha256": "30c87ee44412fee4f50173716a6bb7c6941d3021853f0adab4a249da8a9455ab"
      },
      "vendor": {
        "path": "/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z/vm-inputs/vendor",
        "size": 562388992,
        "sha256": "5cc4267517ded28f1cf60a5d2b6e8b88b15d8520de1f55a39de7764e7929a877"
      },
      "kernel": {
        "path": "/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z/vm-inputs/kernel",
        "size": 22608880,
        "sha256": "08a14c2f0961cd867e3c9223f4718e3d2315b21a29b5c559d9fdbe07ef701eb7"
      },
      "initramfs": {
        "path": "/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z/archive-2150/initramfs.cpio.gz",
        "size": 260225054,
        "sha256": "10594503ee7edf38d8daf0284f3439402ec78b16140e017c569952248ee111db"
      }
    }
  },
  "manifestSha256": "9b583fc2fa79a054ed3a2e8124da2eb5c345eb9466ba2978c58268c459d3e790",
  "protectedSourcePins": {
    "tools/foreign-runtime/android-stock-proof/README.md": "c5dabd4a952aa7c21733f1697007686c6b43d9c8bb24d626adcd5f72b4871454",
    "tools/foreign-runtime/android-stock-proof/RUNTIME-INPUTS.md": "f19815b4a618636b18f913aee09577425fee26a06ec63342589853aed24f2abf",
    "tools/foreign-runtime/android-stock-proof/boot_plan.py": "6c378966529a0c8e53890c3b99ca3f97f14b2d83650268f685654c13eb49a485",
    "tools/foreign-runtime/android-stock-proof/copy_stage.py": "6bc80119cba3a72971947971406db84bfe27b023010b192c2d3067fe90d71a4e",
    "tools/foreign-runtime/android-stock-proof/guest-init.sh": "b12a17cfe8bb3c3783c611a33adffc4edef61c7f03711b8b1edb1a65497f6346",
    "tools/foreign-runtime/android-stock-proof/guest.py": "13c6da6500787a28408dc515c284dd35898c343ef8d814fdf2c4c5c0f8a5209f",
    "tools/foreign-runtime/android-stock-proof/input_links.py": "644fc11a40ba1f3aa7c569f77e5f18e322e21a9c23d7658defda75764add87b9",
    "tools/foreign-runtime/android-stock-proof/laptop-inputs.json": "d06728d6864803cc8d9de6ae226cd9b790f8c8e3c0330ca5ee31106db8ec6462",
    "tools/foreign-runtime/android-stock-proof/prepare_overlay.py": "3dfa3075dcdf853c87f5c56053e56bafe9773aa0cb4a26f2fa3c3f32cf09ad0a",
    "tools/foreign-runtime/android-stock-proof/run_vm.py": "4bbb8c9a3836554c78f3f627a74a4101edb4134e40fd30c8a5c0612a8c09a4b5",
    "tools/foreign-runtime/android-stock-proof/runtime-inputs.json": "f7b0b9c32e50e4b29496a73df6753910e934b41997d68c1adb7226584c98c5a2",
    "tools/foreign-runtime/android-stock-proof/scenario.json": "b1dcb1e2850b463884ea4106523dc0a3efa7d05dbf99d0e186bbf41bcb3539d2",
    "tools/foreign-runtime/android-stock-proof/stage_inventory.py": "8f3d1cddee2ece44415a1ed80b1645964ebf4ee0575bed4b444941f6de043d47",
    "tools/foreign-runtime/android-stock-proof/test_boot_plan.py": "bc3b52dfd28aa4976d112f79aea6e1e2caaf1d66ee504ea3ee911b07af3d8d4c",
    "tools/foreign-runtime/android-stock-proof/test_copy_stage.py": "e0c1846bf03446fec7916e148aadb54102bca29bea8833a99edd731b501cbf9c",
    "tools/foreign-runtime/android-stock-proof/test_input_links.py": "e229582ef39c453c48a56e82e2ff9fd8598af73c2b2d718d607d3c24bbcbe82e",
    "tools/foreign-runtime/android-stock-proof/test_stage_inventory.py": "b3c26ae180d1183d7da7556b82fce7ac506f657028b1f0c37b59047cc642eb33",
    "tools/foreign-runtime/android-stock-proof/test_vm_plan.py": "4b475dfbeff6efc4078acacbc369273497a21231c41786f4f4fe9fee09f93b25",
    "tools/foreign-runtime/android-stock-proof/vm_plan.py": "0dde453a7ed2051db7fdb0c3ab71a0da647cdd007ac45a58f409a7363f857deb",
    "tools/foreign-runtime/android-stock-proof/windows.py": "0ca5e11bc840e30cc2517c6df550c22835c1e5a40293d06aa26605a56905d788"
  },
  "inputEvidence": {
    "priorThreeInputEvidence": {
      "path": ".cache/android-vm-image-inputs-20261008/proof.tar.gz",
      "bytes": 7598,
      "sha256": "9f4f5df3a16a41120a090e19a8f4d19aa687a9508629f3271e62ee5242003410",
      "payloads": 15,
      "members": 16
    },
    "newArchiveSHA256": "10594503ee7edf38d8daf0284f3439402ec78b16140e017c569952248ee111db"
  },
  "archiveEvidenceCommit": "pending-current-handoff",
  "runtimeSourceCommit": "86b7f2f7ab4f8c39bc59d8a1f61d14d7ef33e883",
  "qemuSha256": "f580fb233b0de6beae9d5e9be418297a8b8e4d64abec764bb98bde9bff6b7eda",
  "observerPolicy": "Reuse held-Popen/pidfd/start/live-unit/kernel-cap observer; outer observation bound360s; no signals to discovered PIDs. Existing run_vm handles owned QEMU300s/32MiB and final input versions/cleanup. Stop first unexpected result, preserve all outputs, no retry.",
  "guestSourceCommit": "beb2f4dc13cecc3910357547759c7870cfaf4b10",
  "firstAttempt": "failed stock init; original evidence698c7c preserved"
}
~~~
