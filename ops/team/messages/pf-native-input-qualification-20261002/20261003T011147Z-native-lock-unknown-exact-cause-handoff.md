# Real importer Unknown diagnosis: remove incompatible no-lockscreen flag

Exactdesktop5917 and fork55d source inspected. No runtime/test/build/provider/prompt/secret read or productedit. Root reported actual owner/PID/socket admissions passed, importer code6 after3.028s, no catalog; sanitizedattempt2 metadata confirms catalogExistsfalse. No source-store/item-label content read.

Precise root cause: fork main_wayland.cpp427 declares --no-lockscreen; lines602–603 setSupportsLockScreen(false). wayland_server.cpp677–678 gates initScreenLocker on supportsLockScreen; initScreenLocker699–703 alone constructs real SessionLockController and NativeLockDBusInterface. Thus compositor/name/socket may be live while /org/qindaqt/KWin/NativeLock is entirely absent. Neither supervisor --native-power off nor --no-keyring/no-portal creates this compositor-owned endpoint.

QtNativeLockTransport resolves actualnativeowner/PID, subscribes exactowner signals, then calls RequestStateWithReceipt on that uniqueowner/path/interface with32hexnonce. Both emptymethod reply and targeted same-owner nonce-correlated stateReceipt(sbb) are required. Missingobject causes requestfailure, no unlockedreceipt, state remainsUnknown. CLI waits atmost3000ms then nativeAdmittedfalse→OwnerLost(code6). Raising wait, accepting methods/properties alone, emitting fake receipt or selecting syntheticpolicy fixture does not repair absentendpoint and is prohibited.

Smallest existingoperation fix belongs to root: remove ONLY --no-lockscreen from the current actual nested compositor argv; do NOT add --lockscreen (would start initially locked). Leave existing supervisor/private-scope/no-keyring/no-portal, nativeidentity/socket, samehostPID/bus namespace and prompt mounts unchanged. Clean only owned nested attempt before one source-reviewed operational replay; never replace/terminate physical compositor/apps. Real SessionLockController starts m_locked=false, and RequestStateWithReceipt sends actual false,false snapshot; importer may then classify Unlocked, subject to all existing attachment/source lifetime checks. No sourcepatch/rebuild or bypass required. Actual endpoint/receipt/import success remains rootruntimeverification, not inferred here.

The prior suggested preinstallcommand5d54 incorrectly included --no-lockscreen for a CLI thatrequiresNativeLock; this receipt explicitly supersedes that flag recommendation. Existing keyring_legacy_import uses policy_compositor fixture that unconditionally registers its NativeLock adaptor and startslockedfalse, so it never exercises production main_wayland no-lockscreen endpoint exclusion. Actualcore NativeLock tests call the real endpoint; fixturePASS doesnotvalidate this disabled-lock production argv.

Stopat exactcause/nextoperation, no further probes or capability expansion. Ownboard/newmessages only, no resourcelease. Verification source assertions/diffcheck0; no runtime claim.

Source bindings:

[
  {
    "repo": "fork",
    "commit": "55d1f2738723316fae4686468757aef66ce7590b",
    "path": "src/main_wayland.cpp",
    "sha256": "89deb4e620d0e6fa65af9470f0a9a3a9d2f7696dad5b1b78f0f9f1adbb15e46c"
  },
  {
    "repo": "fork",
    "commit": "55d1f2738723316fae4686468757aef66ce7590b",
    "path": "src/wayland_server.cpp",
    "sha256": "c2e6d52a29bbbc70a86108702a51c240aa052c1947fd8769805be59379378019"
  },
  {
    "repo": "fork",
    "commit": "55d1f2738723316fae4686468757aef66ce7590b",
    "path": "qindaqt/session-lock/nativelockdbusinterface.cpp",
    "sha256": "7a509c739df39f8f1faa554d562ba439722dc32407d4b5daa7e3c7a1501a8a7c"
  },
  {
    "repo": "fork",
    "commit": "55d1f2738723316fae4686468757aef66ce7590b",
    "path": "qindaqt/session-lock/sessionlockcontroller.h",
    "sha256": "ff4fb4ce1cc5003adb68cc21833a9abbbe3f70314365662e998be9cb8705f5b4"
  },
  {
    "repo": "desktop",
    "commit": "5917da5c054ccd8ed9fd0be5a2ac9db549a4bb1c",
    "path": "src/services/keyring/app/import_main.cpp",
    "sha256": "003d05ecdbad021fef05fc5a40c587d807338b4022eb749bb1e8c7a5707bc5d2"
  },
  {
    "repo": "desktop",
    "commit": "5917da5c054ccd8ed9fd0be5a2ac9db549a4bb1c",
    "path": "src/services/session_lock_state/src/qt_native_lock_transport.cpp",
    "sha256": "7453b00606d6ea1096995a05a8851e98c4f818d243c1577cdec501aae47cf27e"
  },
  {
    "repo": "desktop",
    "commit": "5917da5c054ccd8ed9fd0be5a2ac9db549a4bb1c",
    "path": "src/services/session_lock_state/src/native_lock_state_monitor.cpp",
    "sha256": "3080dc5b953b7da7f1b164e6a2beb174761f1b0f44ce7f00a849448b7957f13d"
  },
  {
    "repo": "desktop",
    "commit": "5917da5c054ccd8ed9fd0be5a2ac9db549a4bb1c",
    "path": "tests/services/keyring/test_legacy_import.py",
    "sha256": "16161a0267297e93b5af09257a7b08ed8acecf0b52faff8a8626395fa0a10f89"
  },
  {
    "repo": "desktop",
    "commit": "5917da5c054ccd8ed9fd0be5a2ac9db549a4bb1c",
    "path": "tests/services/keyring/policy_compositor.cpp",
    "sha256": "0469acd7a54ba8c3905f385d925f9905931c1f49999ee7b8c5bdc55ac805034c"
  }
]
