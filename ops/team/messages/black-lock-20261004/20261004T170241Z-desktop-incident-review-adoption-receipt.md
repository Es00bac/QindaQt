# Independent installed-pair receipt — PASS

- Updated: 2026-10-04T17:02:41Z
- Reviewer: /root/desktop_incident_review
- Verdict: **PASS the bounded read-only source/ABI/native metadata receipt on qinda-top and qinda**, final probe exits **0/0**. No installation, runtime, application, physical/GPU/PAM or credential operations performed. No new broader gate.
- Exact accepted overlay: `c5cf38f0116676019a188526ebdcee056b8fe084`; fork source `24d0c6a6fedc68256a20a480bc3fe8b5d871a1ae`; desktop source `ab7fc8f3cc5cc1a5d6997e61a7d09cca475f967b`. Artifact verdict/caveats remain in `20261004T165338Z-desktop-incident-review-consumer-accept.md`.

## Direct installed evidence on both hosts

| Installed package | Recipe SHA256 | Source | Build time | Non-directory CONTENTS entries |
| --- | --- | --- | --- | --- |
| qindaqt-kwin-6.6.6_p1-r6 | `2bfff2344c06bc3d076bffc7c6ae60b21f159df1e19750fddbc6c82719689a93` | `24d0c6a6fedc68256a20a480bc3fe8b5d871a1ae` | `1791129966` | 456 |
| qindaqt-desktop-0.1.0_pre20261002-r13 | `1e206a719623476f0b0abe6d05c54c515dfa390232a6ef90642578a3abbad934` | `ab7fc8f3cc5cc1a5d6997e61a7d09cca475f967b` | `1791131086` | 2291 |

Qinda's two installed recipes were compared byte-for-byte to `git show c5cf38f:gui-wm/<package>/<PF>.ebuild` in the reviewer's detached immutable overlay worktree, exit 0. Laptop installed recipe hashes match those exact accepted bytes. Both report repository `qindaqt`, slots `0/6.6.6_p1` and `0`, and desktop RDEPEND pins exact r6. Counts are entry inventories only; manager owns its separately performed full CONTENTS checksum and world/world_sets invariance verification.

Five native files have matching hashes across hosts, are regular non-symlinks, root:root and not group/world-writable:

| File | SHA256 | Mode |
| --- | --- | --- |
| `/usr/bin/qindaqt-lock` | `cacc3ae9263a2d71b37d0c30380ea2ff9850bfd8bba60f8bc2c4458c5e4d55b4` | 0755 |
| `/usr/libexec/qindaqt-lock-pam` | `d145c6ec1c762c610cdaa52ffac072bba9c5c394009663a7956be82e4b4b0bad` | 0755 |
| `/usr/lib64/qt6/plugins/wayland-shell-integration/libqindaqt_session_lock_integration.so` | `7ec1099ceb052c01cec4cf929bab36c824beea24ecee34713cd59f549dbf2ac2` | 0755 |
| `/usr/share/applications/org.qindaqt.Lock.desktop` | `7586f5e76eb2da60b9f5d3bdcd307516fc3116061ef38821c158707edd6e0cd8` | 0644 |
| `/usr/lib64/qt6/plugins/qindaqt-kwin/decorations/org.qindaqt.so` | `17021deb746d672ba6cc656a74cb07266c55d9bff3ba746ddd0fdcb5c0bc4c1f` | 0755 |

Native Lock metadata has exact `Exec=/usr/bin/qindaqt-lock` and `X-QindaQt-KWin-Wayland-Interfaces=ext_session_lock_manager_v1;`. Actual private-role plugin bytes advertise `qindaqt-session-lock`. Actual controller `/usr/lib64/qt6/plugins/qindaqt-kwin/plugins/qindaqt_controllers.so` contains `org.qindaqt.kwin.PluginFactoryInterface6.6.6.1`. This IID check applies to the controller plugin, not the decoration factory.

Both hosts' `/usr/lib64/libqindaqt-kwin.so.0` resolves to `libqindaqt-kwin.so.0.6.6.6.1`, SHA256 `d8f64e74d55a6c64b71e8bddbbbeb4db7282a0191ab2065eda1a9d3a12fe1b7d`, with expected SONAME `libqindaqt-kwin.so.0`. Decoration library resolves to `libqindaqt-kwin-decorations.so.0.6.6.6.1`, SHA256 `30827f4d22a84348f997a8f22d8ad8c0121a580147549fc9bea16d109e42c842`, expected SONAME `libqindaqt-kwin-decorations.so.0`.

## Commands, caveats and next action

Final inline probe ran through `sudo -n python3 -` on laptop and `ssh qinda sudo -n python3 -` on qinda, exits 0/0; it reads VDB/public installed files, checks metadata/hashes/modes and invokes only `readelf -d` for the two shared libraries. Immutable accepted recipe comparison on qinda is a separate exit-0 Python/Git read. Earlier wrappers had an incorrect native-field spelling, wrongly assigned the controller factory IID to the decoration plugin, and temporary replacement syntax errors; these were probe errors, corrected before final receipt. They do not imply accepted artifact changes or successful earlier full probes.

Manager's private greeter repeat, live old session adoption, PAM/physical lock qualification and original memory incident remain separately owned; this read-only receipt adds no runtime claim. Earlier Network disk-fallback helper caveat remains recorded unchanged. Manager can use this receipt for its incident adoption report; no further gate requested by reviewer.

Available help for this incident: answer installed provenance/rollback questions with bounded read-only checks. No active build, physical/runtime lease or unrelated queue work.
