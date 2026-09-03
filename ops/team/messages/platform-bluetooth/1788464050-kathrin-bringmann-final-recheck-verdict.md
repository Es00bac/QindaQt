# Kathrin Bringmann — independent platform review

- Provider/model: OpenAI Codex `gpt-5.6-sol` (reasoning high)
- Candidate: `7025a1cab90419baf07431e5880bd40ebee2afac`
- Tree: `f1cef09db3fc97bbf91756948c3e05fa3e3f8bcb`
- Parent: `852dfeb4266d6fd3bc96d0ef7eff12015fe4f495`
- Base: `196e69dc42d8761aa95b0f73cefd6ad8d0d25bf0`
- Rejected ancestor: `43a7cb16d4d053b1e05ba4351d986b235676cbde`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-pairing-codex-review`

## Findings ledger

### P0

None.

### P1

None.

### P2

1. **Settings Escape cancellation is disabled by two ambiguous window
   shortcuts in the production host.** `src/apps/settings_center/Main.qml:141`–`145`
   installs an always-enabled Escape `Shortcut` (whose default context is
   `Qt.WindowShortcut`), while
   `src/apps/settings/bluetooth/qml/BluetoothPairingSection.qml:26`–`34`
   installs the same sequence and context while a prompt is active. Qt treats
   both as ambiguous and activates neither, so the documented prompt rejection
   does not occur.

   Exact product reproduction: the scratch executable at
   `/home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/repros/settings-escape-product/settings_escape_product_repro`
   links the same libraries and static QML plugin initialization objects as
   `qindaqt_bluetooth_window_close_tests`, loads the candidate's actual
   `src/apps/settings_center/Main.qml`, supplies an active confirmation prompt,
   and sends Escape under fatal warnings. Run:

   ```sh
   env -u DBUS_SESSION_BUS_ADDRESS \
     DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
     QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1 \
     /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/repros/settings-escape-product/settings_escape_product_repro
   ```

   It exited 0 after printing
   `promptReplies= 0 lastBoolean= false`. Exit 0 here means the defect was
   reproduced. Expected: `promptReplies=1`, with the false confirmation sent
   exactly once. A smaller Qt control reproduced the ambiguity independently:

   ```sh
   env -u DBUS_SESSION_BUS_ADDRESS \
     DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
     QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1 \
     /usr/lib64/qt6/bin/qmltestrunner \
     -input /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/repros/settings-escape \
     -import /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/debug/qml -o -,txt
   ```

   That exited 0 with 3/3 checks passed and printed
   `hostActivations=0 promptActivations=0`.

   The focused tests miss this composition: the Escape assertion in
   `tests/apps/settings/bluetooth/tst_bluetooth_page.cpp:151`–`190` loads the
   Bluetooth page without `Main.qml`, while the full-host Escape assertion in
   `tests/apps/settings/bluetooth/tst_bluetooth_window_close.cpp:220`–`278`
   leaves `pairingPrompt` inactive. Workaround: focus and activate the visible
   Cancel button.

### P3

None.

## Recheck disposition

- Bluetooth1 remained byte-for-byte unchanged from the base. Both XML inputs
  hashed to
  `a4961b2db2886c93cefc436064eaf8cd774458b40b2b87a3c42bce742d3d3d85`,
  and `git diff --exit-code 196e69d..HEAD --
  src/services/bluetooth_service/data/org.qindaqt.Bluetooth1.xml` exited 0.
- The additive Bluetooth2 prompt ID is carried through service, client,
  model, and backend. The private-bus attack accepted only the first of two
  replies to one prompt and rejected an old reply after owner replacement:
  `simultaneous first=0 second=1 pair=0 prompt=1` and
  `replacement oldPrompt=2 newPrompt=3 stalePending=0 staleStatus=1
  staleReason=no-prompt` (status 0 is succeeded; status 1 is rejected).
- Final-adapter loss and shutdown unregister tests passed. After a private-bus
  owner replacement, the new owner observed exactly one registration and one
  shutdown unregister: `replacementOwner registerCalls= 1 unregisterCalls= 1`.
  Source inspection also confirmed unregister is addressed to the stored exact
  unique owner before owner state is changed, so a replacement cannot receive
  an earlier owner's unregister.
- Applet Escape cancellation passed in the actual QML row under fatal warnings.
  Settings' isolated page row also passed, but the production composition
  reproduction above shows its missing negative control.
- Stable Bluetooth2 reasons are `pairing-cancelled` and `prompt-cancelled`, and
  the applet maturity prose now describes the production direct-QtDBus backend.

## Commands and results

- Initial and final `git rev-parse HEAD`, `HEAD^{tree}`, and `HEAD^` returned the
  candidate, tree, and parent above. Initial and final
  `git status --porcelain` produced no output (exit 0).
- Debug configure exited 0:

  ```sh
  cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/debug -G Ninja \
    -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake \
    -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
    -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
    -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
  ```

  Release used `/release` and `-DCMAKE_BUILD_TYPE=Release`, with all other
  arguments identical; exit 0.

- The following exact target list built in both trees with exit 0 (1,282 Ninja
  actions each):

  ```sh
  cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/debug --parallel 3 --target qindaqt_bluetooth_protocol_tests qindaqt_bluetooth_model_tests qindaqt_bluetooth_deterministic_backend_tests qindaqt_bluetooth_client_tests qindaqt_bluetooth_qt_transport_tests qindaqt_bluetooth_activation_tests qindaqt_bluez_adapter_tests qindaqt_bluez_adapter_operations_tests qindaqt_bluez_pairing_tests qindaqt_bluez_backend_mode_tests qindaqt_bluetooth_service_tests qindaqt_bluetooth_lease_owner_loss_tests qindaqt_bluetooth_settings_model_tests qindaqt_bluetooth_settings_adversarial_tests qindaqt_bluetooth_page_tests qindaqt_bluetooth_window_close_tests qindaqt_bluetooth_applet_presentation_tests qindaqt_bluetooth_applet_request_tests qindaqt_bluetooth_applet_controller_tests qindaqt_bluetooth_applet_qml_tests qindaqt_bluetooth_applet_surface_tests qindaqt-bluetooth-service qindaqt-settings qindaqt-shell
  ```

  Release used the identical command with `/release`; exit 0.

- Unreachable-bus Bluetooth selectors:

  ```sh
  env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
    ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/debug \
    -R bluetooth --output-on-failure --no-tests=error
  ```

  Debug passed 31/31 (exit 0, 18.14 s); Release used `/release` and passed
  31/31 (exit 0, 14.53 s).

- Fatal-warning UI selectors:

  ```sh
  env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
    QT_FATAL_WARNINGS=1 \
    ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/debug \
    -R '^(qindaqt\.settings-bluetooth-(page|window-close)|qindaqt\.bluetooth-applet-(offscreen|surface))$' \
    --output-on-failure --no-tests=error
  ```

  Debug passed 4/4 (exit 0); Release used `/release` and passed 4/4 (exit 0).

- All five prior structural checks were rerun:

  ```sh
  python3 /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-codex/repros/contract_checks.py wire-abi
  python3 /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-codex/repros/contract_checks.py stale-api
  python3 /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-codex/repros/contract_checks.py unregister
  python3 /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-codex/repros/contract_checks.py escape
  python3 /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-codex/repros/contract_checks.py reasons
  ```

  Each exited 0. They printed, respectively: the same v1 signature on base and
  candidate with `schemaVersion=2`; no v1 prompt-reply inputs or missing
  lineage (the methods moved to v2); one `RegisterAgent` and one
  `UnregisterAgent`; both source-shape Escape checks true; and empty v1 reason
  lists (the tokens moved to the v2 reference). The last four structural
  outputs were supplemented by executable product/private-bus tests rather than
  treated as standalone proof.

- Prompt fencing/owner replacement scratch probe:

  ```sh
  cmake -S /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/repros/prompt-fencing \
    -B /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/repros/prompt-fencing/build -G Ninja
  cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/repros/prompt-fencing/build --parallel 3
  env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
    /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/repros/prompt-fencing/build/prompt_fencing_repro
  ```

  Configure, build, and run each exited 0; observed values are recorded above.

- The Debug `qindaqt_bluez_pairing_tests` selectors
  `stalePromptReplyCannotAuthorizeReplacement`,
  `unregistersAgentOnAdapterLossAndShutdown`, and `ownerLossFailsClosed` were
  run individually with `-v1` and unreachable host-bus variables; each exited 0
  with 3/3 QtTest functions passed.
- Product Settings reproduction source:
  `/home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/repros/settings-escape-product/repro.cpp`.
  It was compiled by replaying the exact compile and link commands emitted by
  `ninja -C /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/debug
  -t commands qindaqt_bluetooth_window_close_tests`, substituting only the
  scratch source, object, and output paths; compile and link both exited 0.
  The run command and result appear in the P2 reproduction.
- A direct `qmltestrunner` attempt to load full `Main.qml` exited 134 under
  fatal warnings because that generic runner cannot load the target's static
  `QindaQt.SettingsApp.Customize` plugin. The linked product reproduction above
  supplies those exact target plugin objects and ran successfully.
- `./tools/validate-docs`: exit 0; 140 Markdown documents and navigation
  validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-btpair-r2-codex/site`:
  exit 0.
- `./tools/check-source-shape`: exit 0; 2,421 files checked and 0 skipped;
  threshold warnings only.
- `git diff --check && git diff --check 43a7cb1..HEAD && git diff --check
  196e69d..HEAD`: no output, exit 0.
- No JSON changed, so `python3 -m json.tool` was not applicable.
- No nested compositor/session row, ambient host D-Bus, hardware, uinput,
  network, or other forbidden surface was used.

VERDICT REJECT P0/P1/P2/P3=0/0/1/0
