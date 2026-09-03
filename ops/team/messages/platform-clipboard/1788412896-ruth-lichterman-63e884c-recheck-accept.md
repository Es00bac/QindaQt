# Ruth Lichterman — exact-candidate review of Clipboard C1 repair

- **Reviewer/persona:** Ruth Lichterman (`ruth-lichterman`), independent repair reviewer replacing Evelyn Berezin after provider exhaustion
- **Provider/model:** OpenAI Codex `gpt-5.6-sol`, reasoning high
- **Candidate SHA:** `63e884cfa2216d7dc492407e30c7ce28b8512ac0`
- **Candidate tree SHA:** `9dac14a4521ebf8c4e50a90004865d4dcf943da7`
- **Parent SHA:** `8db8ed1f3a35991a690c10ad0d8303c7599791f0`
- **Base SHA:** `ce9228d9694622d503d92a38d01986f8f124f188`
- **Rejected product ancestor:** `405577cc964dd1282a9210af282650a132391056`
- **Review worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/clipboard-service-c1-k3-review`
- **Build root:** `/home/cabewse/work_SPaC3/builds/qindaqt/review-clip-r2-codex`
- **Hygiene:** `HEAD`, tree, parent, and merge-base identities matched before and after review. `git status --porcelain` was empty before and after. No worktree path was edited; scratch evidence is below the assigned build root.

## Findings ledger

### P0 — none

### P1 — none

The prior P1-1 is closed on the immutable candidate.

- `data/settings/schema-v1.json:228-231` and `data/settings/schema-v2.json:249-252` both ship Boolean `false`.
- `src/services/clipboard_service/app/clipboard_history_consent.cpp:15-27` requires the value to be Boolean `true` and its source to be the exact string `user-overrides`; `main.cpp:50-53` feeds only that decision into `ClipboardHost`.
- A scratch executable loaded each shipped schema through the real `SettingsSchema`/`LayeredSettings` implementation and called the production consent function. For both schema versions, first start resolved `false/system-defaults/consent=false`; a forced `true/profile-defaults` resolved `consent=false`; and `true/user-overrides` resolved `consent=true` (exit 0). The focused host row separately proves a false opt-in keeps adapter capture disabled even when an offer arrives, and that accepted opt-in still requires authenticated-unlocked truth.
- Debug and Release passed all 23 Settings rows, all four appearance rows, and all four requested notification/Settings1 rows after the default change.

### P2 — none

The prior P2-1 is closed on the immutable candidate.

- `src/services/clipboard_service/src/clipboard_service_object.cpp:61-96` admits at most 64 callers, looks up retained request identities before eviction, and evicts the oldest inserted id when a fresh id arrives at the per-caller limit.
- `tst_clipboard_request_cache.cpp:36-76` drives 66 fresh ids (the 64-result cap plus two), requires every fresh result to succeed, then repeats retained id 66 and requires byte-for-byte `OperationResult` equality. The focused row passed in Debug and Release; a direct Debug invocation of that single test function also passed 3/3, exit 0.
- `docs/wiki/reference/clipboard1-v1.md:48-61` matches the code: FIFO by insertion, retained exact replay/conflict behavior, an evicted identity treated as fresh, and no inference of replay safety after timeout or eviction.

### P3 — none

All three earlier P3 observations are closed rather than merely carried.

- The caller bound check is now `>=` before cache insertion, and the 65-caller private-bus negative row passes.
- The Wayland adapter retains at most 16 unselected offers, accepts at most 64 advertised names, and refuses an overlong name before opening a receive pipe. The added fake-server rows pass.
- Manager-global removal and compositor disconnect now have executable negative controls; both withdraw availability and prevent capture. Those rows pass in both Clipboard selectors.

No unrelated product behavior moved in repair commit `63e884c`: its 21 changed paths are the two authorized schemas, Clipboard service/Wayland repairs, focused tests, and directly affected documentation/build lists. It makes no shared top-level registry edit. Source shape reports the repaired adapter at 499 non-blank lines, below the 500-line decomposition-review threshold.

## Commands and results

`<ROOT>` below is `/home/cabewse/work_SPaC3/builds/qindaqt/review-clip-r2-codex`.

### Identity and scope

```sh
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git merge-base HEAD main
git merge-base HEAD 405577cc964dd1282a9210af282650a132391056
git status --porcelain
git diff --stat 405577c..HEAD
git diff --name-status 405577c..HEAD
git show --stat --oneline HEAD
```

Exit 0. Identities were candidate `63e884c…`, tree `9dac14a…`, parent `8db8ed1…`, base `ce9228d…`, and rejected ancestor `405577c…`; status was empty.

### Fresh configuration

The assigned Debug, Release, site, and scratch subdirectories were emptied first with `find <exact paths> -mindepth 1 -delete`.

```sh
cmake -S . -B <ROOT>/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

The identical command with `<ROOT>/release` and `CMAKE_BUILD_TYPE=Release` was also run. Both exited 0 and generated fresh trees. Only the repository's existing mixed-prefix CMake runtime-path warnings appeared.

### Focused builds

For both `<ROOT>/debug` and `<ROOT>/release`:

```sh
cmake --build <profile> --parallel 3 --target \
  qindaqt-clipboard-host \
  qindaqt_clipboard_protocol_tests qindaqt_clipboard_client_tests \
  qindaqt_clipboard_service_tests qindaqt_clipboard_private_bus_tests \
  qindaqt_clipboard_request_cache_tests qindaqt_clipboard_settings_consent_tests \
  qindaqt_clipboard_wayland_adapter_tests \
  qindaqt_settings_schema_tests qindaqt_layered_settings_tests \
  qindaqt_settings_persistence_tests qindaqt_settings_migration_tests \
  qindaqt_settings_protocol_tests qindaqt_settings_protocol_dbus_tests \
  qindaqt_settings_repository_tests qindaqt_settings_service_lifecycle_tests \
  qindaqt_settings_service_process_lifecycle_tests qindaqt_settings_client_tests \
  qindaqt_settings_commit_reply_validation_tests qindaqt_do_not_disturb_controller_tests \
  qindaqt_qt_settings_transport_tests qindaqt_qt_settings_transport_adversarial_tests \
  qindaqt_settings_route_registry_test qindaqt_settings_navigation_controller_test \
  qindaqt_settings_navigation_page_test qindaqt-settings \
  qindaqt_appearance_values_tests qindaqt_appearance_preview_tests \
  qindaqt_appearance_model_tests qindaqt_appearance_page_tests \
  qindaqt_notification_quieting_bridge_tests
```

Both exited 0, 557/557 steps. The first Debug Clipboard selector then exposed that the handoff target list omitted the four pre-existing C0 test executables: exit 8, 10 rows passed and four were Not Run. The first multiplexed Release invocation's completion status was not retained, so no result is inferred from it. I built the exact missing closure in each profile:

```sh
cmake --build <profile> --parallel 3 --target \
  qindaqt_clipboard_media_tests qindaqt_clipboard_history_tests \
  qindaqt_clipboard_history_lineage_tests qindaqt_clipboard_codec_tests
```

Both exited 0, 16/16 steps. The identical Clipboard selectors were then rerun and fully observed as green.

### Runtime selectors

Each command was run against both `<ROOT>/debug` and `<ROOT>/release`:

```sh
ctest --test-dir <profile> -R '^qindaqt\.clipboard-' --output-on-failure --no-tests=error
ctest --test-dir <profile> -R '^qindaqt\.settings' --output-on-failure --no-tests=error
ctest --test-dir <profile> -R '^qindaqt\.appearance-(values|preview|settings-model|page)$' --output-on-failure --no-tests=error
ctest --test-dir <profile> -R '^qindaqt\.(notification-quieting-settings-bridge|notification-quieting-controls-offscreen|notification-(surfaces|focus)-offscreen)$' --output-on-failure --no-tests=error
```

Final results:

| Profile | Clipboard | Settings | Appearance | Notification |
| --- | ---: | ---: | ---: | ---: |
| Debug | exit 0, 14/14 | exit 0, 23/23 | exit 0, 4/4 | exit 0, 4/4 |
| Release | exit 0, 14/14 | exit 0, 23/23 | exit 0, 4/4 | exit 0, 4/4 |

The Clipboard rows use only an in-process fake Wayland server and private `dbus-daemon` instances rooted under the build tree. No host session/system bus was contacted.

Direct cache evidence:

```sh
<ROOT>/debug/tests/services/clipboard_service/qindaqt_clipboard_request_cache_tests \
  evictsOldestResultsWithoutBlockingFreshIds -v1
```

Exit 0, 3 passed, 0 failed, 0 skipped.

### Shipped-schema consent reproduction

The scratch program and binary live at `<ROOT>/scratch/consent_repro.cpp` and `<ROOT>/scratch/consent_repro`. It was compiled against the candidate's Debug `libqindaqt_settings.a` plus the exact production `clipboard_history_consent.cpp`; an initial non-PIC link attempt exited 1 with the platform Qt protected-symbol relocation diagnostic, and the corrected `-fPIC -no-pie` compile exited 0.

```sh
<ROOT>/scratch/consent_repro
```

Exit 0. Observed:

```text
schema-v1.json initial value=false source=system-defaults consent=false
schema-v1.json profile true value=true source=profile-defaults consent=false
schema-v1.json user true value=true source=user-overrides consent=true
schema-v2.json initial value=false source=system-defaults consent=false
schema-v2.json profile true value=true source=profile-defaults consent=false
schema-v2.json user true value=true source=user-overrides consent=true
```

### Static gates

| Command | Exit | Result |
| --- | ---: | --- |
| `./tools/validate-docs` | 0 | 118 Markdown documents and `mkdocs.yml` navigation validated |
| `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site` | 0 | documentation built |
| `./tools/check-source-shape` | 0 | 1,806 files checked; only the two pre-existing out-of-lane warnings at `tests/compositor/CMakeLists.txt` (500) and `tests/services/display_color_model/tst_color_model.cpp` (539) |
| `git diff --check` | 0 | clean |
| `git diff 405577c..HEAD --check` | 0 | clean repair delta |
| `git show --check --oneline --stat HEAD` | 0 | candidate clean |
| `python3 -m json.tool data/settings/schema-v1.json` | 0 | valid JSON; repaired default shown as `false` |
| `python3 -m json.tool data/settings/schema-v2.json` | 0 | valid JSON; repaired default shown as `false` |

Final `git rev-parse` identities still matched and final `git status --porcelain` was empty. No `tests/session` nested-compositor row, host D-Bus service, hardware, uinput, or network action was run.

## Verdict

The authorized schema and host consent repairs close P1-1 with defense in depth, FIFO request-result eviction closes P2-1 without breaking retained idempotence, and all three P3 gaps have executable repairs. The documentation describes exactly the retained/evicted request semantics and consent trust boundary that the code now implements. No new blocking or precision finding was found.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
