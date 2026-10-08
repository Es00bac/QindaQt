# Archive2340 actual PASS; eleventh VM proposal only

Root's exact one archive grant executed once. Original unit qindaqt-android-diagnostic-archive-20261008T2340Z.service invocationac39189b0a2242b583b0c4c188b6083c, heldouter2723829/start11947047, main2723840/start11947061. Observer0/84.821540s heldouterreaped/absent, finalunitsettled, sixactual livecaps. Archive0/84.441463s; cpio/gzip/gzip-test all0, originalinventory/cloneinventory/sourcepinsstable. Independent ledger comparison:7296records, ONLYproof/windows.py changed;7295identical toqualified2325. Earlier sandboxed /proc read reportedabsent while actualhostunitrunning; that limited-namespace observation is retained in active.json and superseded by actual hostidentities/observer live+terminal receipts. No falseearlyrelease; actualheavyreleased onlyafteroriginalobserverterminal.

Newroot0400initrd262661573bytes SHA2564f8a633b5327d8810be330ba8689b23e556922b0a951ca33ae1a38b2b334b5b7. Rawcpio660558848bytes SHA31b42c90942db966120555c3f517c0ce0e0c33ad33bfa8f58da730d25d731f96.
Actualdiagnosticinventory70afa35debf3fe83a57899bb0ae897064847200ddf6a4185ca42b1a176f4c787; diagnosticplan c6a9f92cf061ef960c02675fb63b05a1d97b9a969dbd2dd76350c8c0bba5ad83.
Immutableproof .cache/android-launch-archive-proof.tar.gz375237bytes SHA04e8afc3beea6c66e7402443cd2ea48f7ccca32e4314b2d84671e1011fe1e0c6,17members/16indexedpayloads.

Requested nextgate ONE eleventhVM afterrootreview. Plan6a546abf6845cba9ae6bffc29732c9046be1a7869328d7b9c2b240c3e51fd9b0; protectedmanifest e2f1a5cc38fff9a33609a50d41e2cd30976efdbe05bf0f862b8c2e4ca5c0b7ff. Allthreeoriginalsystem/vendor/kernel andQEMU/source21readerpins unchanged. Onlynewinitrd plusfreshnames. Same330unit300inner360observer/4CPU5GiB/no swap/tasks256/0-7/nice10. No gueststarted inthishandoff. No automaticretry.

TenthFAILED preserved despitevalidterminalresult/cleanup, Calculatorobserved/Clocklaunch10stimeout. Thisnewarchivecontains reviewed328fcf exactfixed-app-onlyshared190slaunchbudget. Two-windowcriteria, ownerfence, terminaldrain, cleanup/inputtruth unchanged. Render/input/resize/close/relaunch, platform/appidentity andhostAudio1 stillnextrequirements. PrivatePulse is startup-only.


## plan.json

```json
{
  "schema": 1,
  "kind": "prospective-eleventh-stock-android-fixed-launch-budget-dispatch",
  "executed": false,
  "host": "qinda-top",
  "unit": "qindaqt-android-stock-eleventh-20261008T2345Z.service",
  "argv": [
    "sudo",
    "-n",
    "systemd-run",
    "--unit=qindaqt-android-stock-eleventh-20261008T2345Z.service",
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
    "/var/tmp/qindaqt-android-laptop-stage-20261008T2250Z/diagnostic-inputs-2340/eleventh-manifest.json",
    "/var/tmp/qindaqt-android-laptop-stage-20261008T2250Z/vm-eleventh-2345"
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
        "path": "/var/tmp/qindaqt-android-laptop-stage-20261008T2250Z/archive-2340/initramfs.cpio.gz",
        "size": 262661573,
        "sha256": "4f8a633b5327d8810be330ba8689b23e556922b0a951ca33ae1a38b2b334b5b7"
      }
    }
  },
  "manifestSha256": "e2f1a5cc38fff9a33609a50d41e2cd30976efdbe05bf0f862b8c2e4ca5c0b7ff",
  "protectedSourcePins": {
    "tools/foreign-runtime/android-stock-proof/README.md": "91e5fc8980e7f8a72db0c4f3217e7608a03f267f0d7c4560fff8fc7ac0fa90d9",
    "tools/foreign-runtime/android-stock-proof/RUNTIME-INPUTS.md": "f19815b4a618636b18f913aee09577425fee26a06ec63342589853aed24f2abf",
    "tools/foreign-runtime/android-stock-proof/boot_plan.py": "6c378966529a0c8e53890c3b99ca3f97f14b2d83650268f685654c13eb49a485",
    "tools/foreign-runtime/android-stock-proof/copy_stage.py": "6a75744db2106316fa0a7d2b6a5395893384601acac9a1750f2877d833847f58",
    "tools/foreign-runtime/android-stock-proof/guest-init.sh": "b12a17cfe8bb3c3783c611a33adffc4edef61c7f03711b8b1edb1a65497f6346",
    "tools/foreign-runtime/android-stock-proof/guest.py": "8014220c46558e8f3f89ee48fb183dbf6adc30185e874aa48e8e208681142b7a",
    "tools/foreign-runtime/android-stock-proof/input_links.py": "644fc11a40ba1f3aa7c569f77e5f18e322e21a9c23d7658defda75764add87b9",
    "tools/foreign-runtime/android-stock-proof/laptop-inputs.json": "d06728d6864803cc8d9de6ae226cd9b790f8c8e3c0330ca5ee31106db8ec6462",
    "tools/foreign-runtime/android-stock-proof/prepare_overlay.py": "3dfa3075dcdf853c87f5c56053e56bafe9773aa0cb4a26f2fa3c3f32cf09ad0a",
    "tools/foreign-runtime/android-stock-proof/run_vm.py": "4bbb8c9a3836554c78f3f627a74a4101edb4134e40fd30c8a5c0612a8c09a4b5",
    "tools/foreign-runtime/android-stock-proof/runtime-inputs.json": "f7b0b9c32e50e4b29496a73df6753910e934b41997d68c1adb7226584c98c5a2",
    "tools/foreign-runtime/android-stock-proof/scenario.json": "b1dcb1e2850b463884ea4106523dc0a3efa7d05dbf99d0e186bbf41bcb3539d2",
    "tools/foreign-runtime/android-stock-proof/stage_inventory.py": "8f3d1cddee2ece44415a1ed80b1645964ebf4ee0575bed4b444941f6de043d47",
    "tools/foreign-runtime/android-stock-proof/test_boot_plan.py": "bc3b52dfd28aa4976d112f79aea6e1e2caaf1d66ee504ea3ee911b07af3d8d4c",
    "tools/foreign-runtime/android-stock-proof/test_copy_stage.py": "e0c1846bf03446fec7916e148aadb54102bca29bea8833a99edd731b501cbf9c",
    "tools/foreign-runtime/android-stock-proof/test_guest_audio.py": "7cd0d43e7fdd91fc9b3954bc8395773988915a43d1c697ba8a0adb85e2b0880f",
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
    "newArchiveSHA256": "4f8a633b5327d8810be330ba8689b23e556922b0a951ca33ae1a38b2b334b5b7",
    "newArchiveProof": {
      "path": ".cache/android-launch-archive-20261008/proof.tar.gz",
      "sha256": "04e8afc3beea6c66e7402443cd2ea48f7ccca32e4314b2d84671e1011fe1e0c6",
      "size": 375237
    }
  },
  "archiveEvidenceCommit": "current immutable handoff",
  "runtimeSourceCommit": "03f4969bd216e1d21fb8fd06d67c08cd2c512b63",
  "qemuSha256": "f580fb233b0de6beae9d5e9be418297a8b8e4d64abec764bb98bde9bff6b7eda",
  "observerPolicy": "Reuse held-Popen/pidfd/start/live-unit/kernel-cap observer; outer observation bound360s; no signals to discovered PIDs. Existing run_vm handles owned QEMU300s/32MiB and final input versions/cleanup. Stop first unexpected result, preserve all outputs, no retry.",
  "guestSourceCommit": "e87f73395ba0f7f18fa39902d0070fc692dde292",
  "firstAttempt": "failed stock init; original evidence698c7c preserved",
  "secondAttempt": "failed libgcc lookup; proof50df86 preserved",
  "thirdAttempt": "PermissionError after stock init; cleanupqualified true; proofee5759 preserved",
  "fourthAttempt": "LXC RUNNING timeout; nested socket and image mounts observed; cleanupqualified; proofd973d5a7 preserved",
  "fifthAttempt": "actual stock LXC rootfs mountpoint ENOENT; proofd2eeac preserved",
  "sixthAttempt": "actual mandatory Pulse bind missing; proof23b8bdff preserved",
  "acceptanceScope": "Same strict two-window/current-owner/geometry criteria. Only exactfixed Calculator/Clocklaunches andbootquery use remainingshared190s. Noindividualretry; allothercommands10s. Guestdrain/parser/cleanupunchanged; renderinput/resizeclose/identity/Audio1unqualified.",
  "windowSourceCommit": "328fcf22be3ac061d218e51289e449edc13b44b4",
  "windowSourceSha256": "14e0a0b5d9046e49b767c93d5be8ef8ce02a76d664f8a9e77b052fedf0fd483b",
  "seventhAttempt": "privateAudioEndpointReady/containerRUNNING/currentCompositorWindows baseline; first10sbootpropertytimeout, no app launch; proof66ed578 preserved",
  "guestSourceSha256": "d8a9e638158de6208bcec8b293da5a0f9ccf4efb691978f8e95bcbfb1ed0b512",
  "eighthAttempt": "two stock windows observed in valid WINDOWS record; terminal RESULT interleaved with PID1panic and strict hostparser refused; proof03822195 preserved",
  "ninthAttempt": "two stock windows observed; ttyname inheritedconsole ENOTTY before finalpublication, overallFAILED; proof3f7e7c6d preserved",
  "archiveObserverCaveat": "Archive2340 original held observer0/84.822s, outerreaped/absent and unitsettled; sixlivecaps. Earlier2325observerfailure remains separately preserved.",
  "tenthAttempt": "ONE intact terminal result; Calculator only, Clock command10s timeout; QEMU0/cleanupqualified/inputsstable; proofd5a3be47 preserved"
}
```

## manifest.json

```json
{
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
      "path": "/var/tmp/qindaqt-android-laptop-stage-20261008T2250Z/archive-2340/initramfs.cpio.gz",
      "size": 262661573,
      "sha256": "4f8a633b5327d8810be330ba8689b23e556922b0a951ca33ae1a38b2b334b5b7"
    }
  }
}
```

## argv.json

```json
[
  "sudo",
  "-n",
  "systemd-run",
  "--unit=qindaqt-android-stock-eleventh-20261008T2345Z.service",
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
  "/var/tmp/qindaqt-android-laptop-stage-20261008T2250Z/diagnostic-inputs-2340/eleventh-manifest.json",
  "/var/tmp/qindaqt-android-laptop-stage-20261008T2250Z/vm-eleventh-2345"
]
```
