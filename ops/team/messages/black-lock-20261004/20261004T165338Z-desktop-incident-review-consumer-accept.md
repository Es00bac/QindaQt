# Exact completed consumer artifact review — ACCEPT

- Reviewer: /root/desktop_incident_review; independent review only, no speech, installation, application/PAM execution, physical session/GPU changes, credentials or signing-key access.
- Updated: 2026-10-04T16:53:38Z
- Verdict: **ACCEPT the exact signed r6/r13 artifact pair for the manager's prepared incident adoption gate**, with the bounded Network disk-fallback contract caveat below. This does not claim independent live greeter/PAM/physical qualification or full consumer CTest completion.

## Exact candidates and artifacts

- Accepted overlay commit: `c5cf38f0116676019a188526ebdcee056b8fe084`; tree `ebf9fbf335c840a3a4ffb82c430e3fb2fa74f91d`. Prior recipe/archive review: `20261004T162654Z-desktop-incident-review-package-accept.md`.
- Fork source: `24d0c6a6fedc68256a20a480bc3fe8b5d871a1ae`, exact deployed parent `dd74a6c1cc3fb79d8d33dc7bc48aa2e94a396698`; tree `3a257b8d991777247970c83ce9ae8ca265448db3`. Prior independent source verdict/tests: `20261004T160126Z-desktop-incident-review-parser-accept.md`.
- Desktop source: `ab7fc8f3cc5cc1a5d6997e61a7d09cca475f967b`, descendant of installed source `2b5db406bbbbfbd1d3feb4573f64c9af1e3b298d`.
- Actual desktop artifact on qinda: `/var/cache/binpkgs/gui-wm/qindaqt-desktop/qindaqt-desktop-0.1.0_pre20261002-r13-1.gpkg.tar`.
  - SHA256 `93061c6fc0280fd7cab6da483e6d2984fc8fdb76ff38dc0d5200b6211e24c9af`; **60,856,320 bytes**.
  - Signed metadata: CATEGORY `gui-wm`, PF `qindaqt-desktop-0.1.0_pre20261002-r13`, SLOT `0`, BUILD_TIME `1791131086`. Metadata has no SRC_URI or BUILD_ID key; source is bound by the signed exact recipe, and filename's package instance is `-1`.
- Actual fork artifact on qinda: `/var/cache/binpkgs/gui-wm/qindaqt-kwin/qindaqt-kwin-6.6.6_p1-r6-1.gpkg.tar`.
  - Previously independently verified SHA256 `47c9d0618c041b954c99ded844e27c265059b7d12df8ad092054f9843d460974`.

## Verification and direct evidence

1. **Actual consumer GPKG signature/checksum verification: exit 0.** Used the installed Portage `gpkg` implementation, signatures required, signature-ignore/signing disabled in a cloned child config, copied only `pubring.kbx`/`pubring.gpg`/`trustdb.gpg` when present into a temporary verification home, and dropped verifier identity to nobody/nogroup. No private signing material copied/read and no image installation/extraction. Container signature exists; full `_verify_binpkg()` succeeds. An initial wrapper failed after this success because it assumed optional SRC_URI metadata; corrected metadata-only wrapper exits 0.
2. **Signed recipe binding: exit 0.** Embedded desktop ebuild is byte-identical to the accepted c5cf38 recipe. It pins source ab7fc8, contains no signed-base reuse, and actual RDEPEND pins `=gui-wm/qindaqt-kwin-6.6.6_p1-r6:=`. Both source archives/Manifests were independently verified in the prior exact recipe gate.
3. **Actual staged payload/static ELF verification: exit 0.** Stage `/var/tmp/portage/gui-wm/qindaqt-desktop-0.1.0_pre20261002-r13/image` contains **2,287 regular files, 4 symlinks, 178,641,720 regular-file bytes**. Scanned **90 ELF files, 694 DT_NEEDED entries and 195 RPATH/RUNPATH search entries**: no source/developer/Portage-workdir path leaks. Compositor depends on native `libqindaqt-kwin.so.0` and `libqindaqt-kwin-decorations.so.0`; actual compositor and controller plugin IID is `org.qindaqt.kwin.PluginFactoryInterface6.6.6.1`.
4. **Existing static stage closure helper: final exit 0**, report **90 ELF, 694 dependencies, 43 QML imports**. Invoked only `desktop_session_stage_closure.verify_stage_closure`; did not run the launch/install CLI. Candidate fork image library directory was first in allowed system roots, followed by the real system roots. Shell's actual required libraries are `libqindaqt_shell_launcher_qml.so` and `libqindaqt_tokens_qml.so`. Supplied 12 embedded module exemptions only after independently authenticating their static target declarations and actual installed consumer ELF URI/class content: Settings Color/Input/Power/Streaming backends and Shell Audio/Bluetooth/GatherOverview/Network/Obs/Power/SmartLights/Voice modules. The first wrapper without these current embedded modules failed; final exact helper invocation passed. This is not a claim that the historical full CTest harness passed unchanged.
5. **Existing narrow package authenticators and native metadata/static identity: pass**, five switcher files and four first-party desktop entries authenticated. `org.qindaqt.Lock.desktop` has exact `/usr/bin/qindaqt-lock` and `ext_session_lock_manager_v1`; PortalCapture has exact `/usr/libexec/qindaqt-portal-capture` and `org.qindaqt.KWin.ScreenShot2`. Actual session-lock QPA plugin advertises `qindaqt-session-lock`. Key paths below are regular, non-symlink and not group/world-writable.

Key stage paths exist:

- `usr/bin/qindaqt-lock` and `usr/libexec/qindaqt-lock-pam`
- `usr/lib64/qt6/plugins/wayland-shell-integration/libqindaqt_session_lock_integration.so`
- `usr/share/applications/org.qindaqt.Lock.desktop` and `org.qindaqt.PortalCapture.desktop`
- `usr/lib64/qt6/plugins/qindaqt-kwin/decorations/org.qindaqt.so`
- Actual full-source controller `qindaqt_controllers.so`, matching factory ABI `6.6.6.1`
- Controls/Input/SettingsApp-Keyring QML metadata; all staged first-party qmldir typeinfo paths exist
- Keyring apps/import/prompt, launch/PAM helpers, user/system units and `usr/lib64/security/pam_qindaqt_keyring.so`

## Bounded Network disk-fallback package-contract caveat

The existing `authenticate_network_qml_package()` disk-only contract **fails**: under `usr/lib64/qt6/qml/QindaQt/SettingsApp/Network`, the five physical `qml/NetworkAccessPointSection.qml`, `qml/NetworkDeviceSection.qml`, `qml/NetworkPage.qml`, `qml/NetworkRadioSection.qml`, and `qml/NetworkSavedSection.qml` files retained in the currently installed package are absent from r13. Flat copies exist. qmldir still lists those nested paths and is byte-identical to installed baseline. Do not report this authenticator or the full package-contract CTest as passing.

This review found no missing required runtime module or registration: qmldir prefers `:/qt/qml/QindaQt/SettingsApp/Network/`; the generated QRC contains all **six** exact nested resource aliases (including `NetworkAccessPointActions.qml`), and the **actual staged backing ELF** has all six matching `/qt/qml/QindaQt/SettingsApp/Network/qml/Network*_qml` compiled cache path symbols plus resource-init symbols and filename data. The actual plugin has URI `QindaQt.SettingsApp.Network`, class `QindaQtSettingsNetworkPlugin`, and DT_NEEDED on `libqindaqt_settings_network_qml.so`. Current Settings Main.qml imports that module rather than directly reading the absent disk paths. The source full recipe installs the five copies flattened. Thus the disk-fallback compatibility/helper mismatch remains an honest caveat rather than a demonstrated required-module defect for this lock repair.

The smallest subsequent packaging repair boundary is the Network QML install destination in `src/apps/settings/network/CMakeLists.txt` (and its package-contract/documentation evidence); preserve or explicitly align the physical nested fallback paths with qmldir. No candidate bytes were changed during review.

## Next action and remaining boundary

Manager may continue the prepared exact matched-pair Portage adoption gate using these immutable artifacts and its separately observed software-only production greeter evidence. The reviewer has not run the full consumer suite, live UI, PAM, installation or physical/session lock qualification. No active review build or runtime lease remains.

Concrete help offer for this incident: inspect the installed pair's read-only provenance, native metadata and shared-library/SDK identity after manager adoption; do not claim unrelated queue work.
