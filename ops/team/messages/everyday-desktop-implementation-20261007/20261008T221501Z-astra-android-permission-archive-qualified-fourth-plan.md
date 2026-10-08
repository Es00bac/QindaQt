# Permission archive2210 qualified; fourth VM pending

Actual once-only archive2210 outer0/21.525s; held2555995/start11467883 reaped/proc absent, main2556000/start11467889. Six actual4CPU5GiB/swap0/tasks256/nice10/affinity0-7 witnesses; final unit inactive/dead/MainPID0. Cpio/gzip/test0 (2.870/7.380/2.469s), original7208 inventory, modified7208 and all20source/tool pins stable. Independently compared inventories: exactly proof/guest.py differs, allother7207 and all modes unchanged. Mechanical driver/observer hash/name-only diff retained. Laptop heavy RELEASED.

New root0400 archive2210/initramfs.cpio.gz260226191bytes SHAd98377f7978a4e323ecc82dbd11a55b8ba51165c26f6f8bdc90d3388a481f2f2; rawcpio654589952 SHA c22b021582f8b45d115b38b5b149b2e3f437dac798dc35bb90ca70465e02b233. Original protectedstage/archives/threefailedVMs remain unchanged.

Proof .cache/android-permission-diagnostic-archive-20261008/proof.tar.gz299124bytes SHAd5a428674ae7f30823441a0671b19f7a35105a404aa83ddfda328cce2d15c739,14members/13payloads independently rehashed on qinda. Actual logs/caps/parity/source/argv retained.

Fourth-plan e97ee8f5652475479ebcab29c34fc248353cb62cadd1768fe55a6442910f9fcc; manifest5d034f42485481cef2dce47ce60d9b6474d9fa135f1c7d6061e30ce1f750563e. Same runtime20source/QEMU/threeinputs/330unit300inner360observer/4CPU5GiB; only fourtharchive/freshunit/output names change. Guest00573 fixed only guestpermissions and bounded diagnostics; hoststage remains0700. Request one fourth diagnostic VM after exactinputreview; no guest executed or app claim.

~~~json
{
  "schema": 1,
  "kind": "prospective-fourth-stock-android-kvm-permission-dispatch",
  "executed": false,
  "host": "qinda-top",
  "unit": "qindaqt-android-stock-fourth-20261008T2220Z.service",
  "argv": [
    "sudo",
    "-n",
    "systemd-run",
    "--unit=qindaqt-android-stock-fourth-20261008T2220Z.service",
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
    "/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z/diagnostic-inputs-2210/fourth-manifest.json",
    "/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z/vm-fourth-2220"
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
        "path": "/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z/archive-2210/initramfs.cpio.gz",
        "size": 260226191,
        "sha256": "d98377f7978a4e323ecc82dbd11a55b8ba51165c26f6f8bdc90d3388a481f2f2"
      }
    }
  },
  "manifestSha256": "5d034f42485481cef2dce47ce60d9b6474d9fa135f1c7d6061e30ce1f750563e",
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
    "newArchiveSHA256": "d98377f7978a4e323ecc82dbd11a55b8ba51165c26f6f8bdc90d3388a481f2f2"
  },
  "archiveEvidenceCommit": "pending-current-handoff",
  "runtimeSourceCommit": "86b7f2f7ab4f8c39bc59d8a1f61d14d7ef33e883",
  "qemuSha256": "f580fb233b0de6beae9d5e9be418297a8b8e4d64abec764bb98bde9bff6b7eda",
  "observerPolicy": "Reuse held-Popen/pidfd/start/live-unit/kernel-cap observer; outer observation bound360s; no signals to discovered PIDs. Existing run_vm handles owned QEMU300s/32MiB and final input versions/cleanup. Stop first unexpected result, preserve all outputs, no retry.",
  "guestSourceCommit": "00573bf502510a035f87173d87cbb46c0f918e82",
  "firstAttempt": "failed stock init; original evidence698c7c preserved",
  "secondAttempt": "failed libgcc lookup; proof50df86 preserved",
  "thirdAttempt": "PermissionError after stock init; cleanupqualified true; proofee5759 preserved"
}
~~~
