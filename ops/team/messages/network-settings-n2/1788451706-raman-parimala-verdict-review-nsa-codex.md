# Raman Parimala — independent Network secret-agent exact-candidate review

- Persona: Raman Parimala, independent platform reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Review completed: 2026-09-03T08:20:34-06:00
- Exact candidate SHA: `f06d2fd88166e5648f092e44cb6190dfcf5f133e`
- Candidate tree SHA: `ce93594bb71145651cdc99bdc1922640faf8be64`
- Parent SHA: `9033df8b8d469a9e072a898bf4a951582c481106`
- Base SHA: `9033df8b8d469a9e072a898bf4a951582c481106`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/network-secret-agent-codex-review`
- Review build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex`
- Review surface: complete `git diff 9033df8..f06d2fd`

The detached worktree matched the candidate and was clean before and after the
review. No product path was edited. Scratch reproductions are confined to the
assigned review build root. No session/compositor row, host bus, real
NetworkManager, hardware, uinput, or network operation was run.

## Findings ledger

### P0

None.

### P1

#### P1-1 — Secret clearing does not overwrite the UTF-16 reply allocation, and standard inbound secret maps are not cleared

- Paths: `src/services/network_secret_agent/src/secret_agent_types.cpp:20-30`,
  `src/services/network_secret_agent/src/secret_agent_object.cpp:20-39`,
  `src/services/network_secret_agent/src/secret_agent_object.cpp:51-62`,
  `src/services/network_secret_agent/src/secret_agent_object.cpp:86-101`, and
  `src/services/network_secret_agent/src/qml_prompt_presenter.cpp:64-80`.
- Contract: `docs/wiki/architecture/network-secret-agent.md:74-81` says secrets
  exist only in editors, short-lived byte arrays, and the reply; that buffers
  are overwritten after dispatch; and that no `QString` secret is retained.
  The standard NetworkManager contract also says `GetSecrets.connection` may
  contain system-owned secrets and `SaveSecrets.connection` contains the
  entire connection including secrets. See the official
  [SecretAgent method contract](https://www.networkmanager.dev/docs/api/latest/gdbus-org.freedesktop.NetworkManager.SecretAgent.html).
- Defect: `wipeVariant()` first obtains an implicitly shared `QString` or
  `QByteArray` copy, then `fill()`/`detach()` overwrites the detached copy and
  finally clears the original `QVariant`. The allocation that held the secret
  is released without being overwritten. The QML-to-C++ path additionally
  materializes a JavaScript object, a `QVariantMap`, and a local `QString`.
  `SaveSecrets` ignores its secret-bearing input map entirely.
- Minimal reproduction (the program compiles the exact candidate helper):

  ```sh
  /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/scratch/build/nsa_review_repros wipe
  ```

  Result: exit 1; `map_empty=true shared_copy_after_wipe="canary-UTF16-S3cr3t"`.
  The shared allocation modeling a serialized/referenced Qt value still
  contains the canary after `wipeSettingsMap()`.
- Expected: every process-owned temporary secret buffer is overwritten before
  release after dispatch, and secret-bearing standard method inputs are
  handled under the same lifetime rule. Observed: the container becomes empty
  but the secret allocation is not overwritten, while `SaveSecrets` performs
  no clearing at all.
- Impact: the central ephemeral-secret invariant is violated; freed Qt/JS heap
  storage can retain credential material after reply or acknowledgement.

#### P1-2 — The documented 65,536-byte request budget is bypassed by nested variant containers

- Path: `src/services/network_secret_agent/src/secret_request_policy.cpp:29-59`.
- Contract: `docs/wiki/architecture/network-secret-agent.md:33-50` requires the
  setting map to be bounded and structurally valid and requires an
  over-budget request to return `NoSecrets` without a prompt.
- Defect: `boundedConnection()` counts property keys and only values for which
  `QVariant::canConvert<QString>()` is true. It neither traverses nor accounts
  for lists/maps and other container-valued variants that are valid inside
  `a{sv}`. An authenticated request can therefore greatly exceed the local
  aggregate budget and still reach the prompt.
- Minimal reproduction:

  ```sh
  /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/scratch/build/nsa_review_repros bounds
  ```

  Result: exit 1; `aggregate_bytes=81920 accepted=true prompt_shown=true completed=false`.
- Expected: an 81,920-byte nested value exceeds the 65,536-byte aggregate
  ceiling, completes as `NoSecrets`, and never opens a prompt. Observed: the
  controller accepts it and opens a prompt.
- Impact: request admission is not fail-closed for malformed/over-budget
  standard setting maps and does not provide the documented resource bound.

#### P1-3 — The only diagnostic canary test discards every real diagnostic and scans test-authored text instead

- Path: `tests/services/network_secret_agent/tst_secret_agent_controller.cpp:245-275`.
- Overclaim: `docs/wiki/development/testing-harness.md:1521` says this row
  proves redacted diagnostics.
- Defect: the installed Qt message handler is an empty lambda. No Qt message is
  appended to `diagnostics`; the sole appended entry is the constant
  `"request completed"` written by the completion lambda. The test does not
  spawn the executable or inspect stdout/stderr, so it covers neither Qt
  diagnostics nor the process's `fprintf` channel.
- Minimal reproduction (same handler and assertion shape, with an emitted
  canary):

  ```sh
  /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/scratch/build/nsa_review_repros diagnostics
  ```

  Result: exit 1;
  `emitted_canary=1 actual_qt_messages_captured=0 scanned_test_entries=1 candidate_assertion_passes=true`.
- Expected: emitting the canary into a diagnostic channel makes the redaction
  assertion fail. Observed: the candidate assertion passes because it never
  sees the emitted message.
- Impact: this is a vacuous test where it is the candidate's only claimed proof
  that credentials do not reach diagnostics; by the review contract this is a
  blocking P1.

### P2

None beyond the blocking findings above.

### P3

#### P3-1 — The prompt proof table states broader keyboard/accessibility coverage than the test directly exercises

`docs/wiki/development/testing-harness.md:1522` attributes accessible states and
show/remember behavior to the prompt row. The implementation contains the
declarative bindings, but `tests/services/network_secret_agent/tst_network_secret_prompt.cpp:104-173`
directly exercises only one accessible name, maximum length, Escape, submit,
and editor clearing. It does not exercise Tab/Shift+Tab, Enter, show/hide echo
mode, checkbox state semantics, or window-close cancellation. This is a proof
precision issue rather than an independently reproduced product failure.

## Review-question results

1. **Secrets never persist or leak — failed.** P1-1 reproduces ineffective
   clearing and identifies ignored standard secret-bearing inputs. P1-3 shows
   the diagnostic-canary proof is vacuous. Static inspection found no explicit
   product logging or persistence API and the boundary rows passed, but those
   facts do not establish the claimed heap-lifetime or all-channel redaction
   contract. Remembered replies use flag `0`; non-remembered replies use
   `NOT_SAVED` (`2`); `AGENT_OWNED` is never set; authenticated Save/Delete
   calls receive typed-void acknowledgements.
2. **Request admission — failed.** Exact-owner comparison, owner-qualified
   `ListConnections`, interaction/known-flag checks, supported setting/hint
   selection, cancellation, timeout, one-shot completion, owner-change
   cancellation/re-registration, and exit unregistration are present and the
   private-bus row passed. P1-2 demonstrates that the setting-map bound is not
   total, so malformed/over-budget input can prompt contrary to the fail-closed
   contract.
3. **Boundary — passed for the inspected surface.** The candidate does not edit
   Network1 production paths. Agent and Settings positive/poison boundary rows
   passed; the Settings observer uses only `isServiceRegistered` plus an
   owner-change watcher for `org.qindaqt.NetworkSecretAgent1` and creates no
   callable D-Bus interface or object path. Shared registries are purely
   additive.
4. **UI — no reproduced product failure, with P3-1 evidence precision.** The
   prompt contains bounded editors, concealed-field echo binding, show/remember
   controls, standard Cancel shortcut, submit-on-accepted, application modality,
   and accessible declarations. The focused offscreen row passed under fatal
   warnings; a direct run with all host display and bus variables unset passed
   4/4 QtTest functions.
5. **Scope and shape — passed.** Changed paths stay within the lane's owned or
   explicitly additive surfaces. ADR-0066 is indexed and in MkDocs navigation.
   Documentation validation, strict MkDocs, source shape, and both diff checks
   passed. No JSON changed. `tests/session/DesktopSessionTests.cmake` is an
   unchanged base issue at 499 nonblank lines in both base and candidate and
   was not run.

## Commands and results

### Identity and cleanliness

```sh
git rev-parse HEAD
git status --porcelain
git rev-parse 'HEAD^{tree}'
git rev-parse HEAD^
git rev-parse 9033df8
```

Exit 0. HEAD `f06d2fd88166e5648f092e44cb6190dfcf5f133e`; tree
`ce93594bb71145651cdc99bdc1922640faf8be64`; parent and resolved base
`9033df8b8d469a9e072a898bf4a951582c481106`; status empty before and after.

### Configure

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/dev -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Exit 0. Configure and generation completed. CMake emitted existing runtime
search-path warnings in unrelated compositor/decorations targets.

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Exit 0, with the same unrelated CMake warnings.

### Focused builds

For each of `dev` and `release`:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/<profile> \
  --parallel 3 --target \
  qindaqt-network-secret-agent \
  qindaqt_network_secret_agent_controller_tests \
  qindaqt_network_secret_agent_prompt_tests \
  qindaqt_network_secret_agent_dbus_tests \
  qindaqt_network_settings_model_tests \
  qindaqt_network_settings_model_adversarial_tests \
  qindaqt_network_page_tests \
  qindaqt_network_secret_agent_presence_tests
```

Debug exit 0, 230/230 build actions. Release exit 0, 230/230 build actions.

### Exact requested selectors

For each of `dev` and `release`:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/<profile> \
  -R '^qindaqt\.(network-secret-agent-|settings-network-)' \
  --output-on-failure --no-tests=error
```

- Debug: exit 0, 7/7 passed, 0 failed.
- Release: exit 0, 7/7 passed, 0 failed.

### Adjacent Network route selectors

For each of `dev` and `release`:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/<profile> \
  -R '^(qindaqt\.network-secret-agent-|qindaqt\.settings-network-secret-agent-presence$|qindaqt\.network-settings-(model|model-adversarial|boundary|boundary-poison)$|qindaqt\.network-page$)' \
  --output-on-failure --no-tests=error
```

- Debug: exit 0, 12/12 passed, 0 failed.
- Release: exit 0, 12/12 passed, 0 failed.

### Direct offscreen/fatal-warning prompt row

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
  -u DBUS_SYSTEM_BUS_ADDRESS QT_QPA_PLATFORM=offscreen \
  QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1 \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/dev/tests/services/network_secret_agent/qindaqt_network_secret_agent_prompt_tests
```

Exit 0; 4 passed, 0 failed, 0 skipped.

### Static gates

```sh
./tools/validate-docs
```

Exit 0; 133 Markdown documents and MkDocs navigation validated.

```sh
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/site
```

Exit 0; strict documentation built successfully.

```sh
./tools/check-source-shape
```

Exit 0; 2,189 source files checked, 0 allowlisted files skipped. It reported
four decomposition-review warnings: 583 lines in
`tests/apps/settings_center/tst_settings_navigation_page.cpp`, 500 in
`tests/compositor/CMakeLists.txt`, 539 in
`tests/services/display_color_model/tst_color_model.cpp`, and 563 in
`tests/shell/audio_applet/tst_audio_applet_controller.cpp`. None is a candidate
path. The unchanged `tests/session/DesktopSessionTests.cmake` appears in the
largest-files report at 499 nonblank lines; direct base/candidate counts were
both 499.

```sh
git diff --check
git diff --check 9033df8..f06d2fd
```

Both exit 0.

```sh
git diff --name-only 9033df8..f06d2fd | rg '\.json$'
```

No output; no changed JSON exists, so `python3 -m json.tool` is not applicable.

### Scratch reproductions

```sh
cmake -S /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/scratch \
  -B /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/scratch/build \
  -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-nsa-codex/scratch/build --parallel 3
```

Final configure/build exit 0. The three modes intentionally exit 1 when the
candidate defect is observed; their exact outputs are recorded under P1-1,
P1-2, and P1-3.

## Verdict

The candidate is rejected because its request bound is bypassable, its claimed
secret overwrite does not overwrite the actual shared Qt allocation and omits
standard secret-bearing inputs, and its only diagnostic-canary proof is
vacuous. Repair should be made on the implementer's candidate branch and the
repaired immutable commit independently rechecked.

VERDICT REJECT P0/P1/P2/P3=0/3/0/1
