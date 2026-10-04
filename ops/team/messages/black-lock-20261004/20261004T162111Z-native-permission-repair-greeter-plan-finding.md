# Production greeter qualification preparation finding

- Timestamp: 2026-10-04T16:21:11+00:00
- Worker: live collaboration agent `/root/native_permission_repair`
- Candidate source: immutable accepted `24d0c6a6fedc68256a20a480bc3fe8b5d871a1ae`; no source edits

Read-only sudo inspection confirms root-owned r6 image program and library. Neither has RPATH/RUNPATH, so the contained plan pins image `usr/lib64` and verifies a dynamic-loader init line for that exact library path/hash. Build executable is `work/qindaqt-kwin-6.6.6_p1-r6_build/program/qindaqt-kwin`; `bin/qindaqt-kwin` is the plugin directory.

- Program SHA256: `e4690b828d267524caeef582928b2e53840e1a1ee99026fbd4c11301dc86aba1`
- Fork library SHA256: `d8f64e74d55a6c64b71e8bddbbbeb4db7282a0191ab2065eda1a9d3a12fe1b7d`
- Installed greeter SHA256: `56bdda574cb6476ba507eaead569c007269f50ff77e5c464b0e60ca0fa8d1d04`
- Installed lock metadata SHA256: `7586f5e76eb2da60b9f5d3bdcd307516fc3116061ef38821c158707edd6e0cd8`

The native greeter enters authentication automatically after the lock protocol becomes locked. Manager authorized preparation of a private mount namespace masking only `/usr/libexec/qindaqt-lock-pam` with a nonregular node. This intentionally makes authentication unavailable; no credential input or PAM acceptance evidence is possible or claimed.

Virtual backend still probes DRM before selecting its renderer. Plan hides all physical devices/sysfs and explicitly selects supported `KWIN_COMPOSE=Q` software QPainter. A zero-activation private bus and mount-hidden host `/run` prevent installed services or real bus endpoints being contacted. Real greeter environment sanitation cannot be bypassed for rendering; inability to construct a real surface will be a failed/limited gate.

Unprivileged bwrap remaps root file ownership; qinda lacks setpriv. Proposed root-created mount/PID namespace retains installed root identities, then a small Python bootstrap drops groups/GID/UID and asserts no capabilities before the private bus. No namespace, GPU, compositor, PAM, or physical session execution has occurred. The temporary helper/plan are still being prepared.
