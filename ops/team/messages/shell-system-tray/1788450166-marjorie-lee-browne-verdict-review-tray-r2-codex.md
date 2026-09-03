# Marjorie Lee Browne — StatusNotifier tray S1 repair-descendant review

- Persona: **Marjorie Lee Browne**, independent shell reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Exact candidate SHA: `4c8e47b28d2d92711bf433c8d2a5afc9be59030f`
- Candidate tree SHA: `47cd0c50922b468f94e1374a287a8c4def526791`
- Parent SHA: `24a27d666085b601135ac6632e5a0ef5f4be1aa9`
- Repair review base / rejected ancestor SHA: `9d1a30b02729c0aba6deba9a43fa67418d283e76`
- Original product base SHA: `ce9228d9694622d503d92a38d01986f8f124f188`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/tray-s1-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex`
- Scratch reproduction root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/repros`
- Review surface: full candidate with focused repair diff `git diff 9d1a30b..4c8e47b`
- Worktree integrity: `git status --porcelain` was empty before review and is empty after review. No product file was intentionally edited, committed, amended, or rebased.

## Verdict

**REJECT.** The descendant closes all ten prior ledger entries with executable registered controls or, for the host signal payload, a valid protocol rebuttal. Debug and Release each pass all seven tray rows and 102 QTest functions, and every static gate passes. One additional private-bus sequence nevertheless shows the monitor reissuing a live owner's generation inside the same watcher epoch after its last item path is retired. That violates the candidate's explicit one-generation-per-owner-per-epoch lineage contract.

`VERDICT REJECT P0/P1/P2/P3=0/1/0/1`

## Findings ledger

### P0

None.

### P1

#### P1-1 — The monitor reissues a still-live owner's generation within one watcher epoch

- Contract: `docs/wiki/shell/status-tray.md:25-27,35-42,172-176` says the transport issues one generation per owner per watcher epoch and shares it across that owner's paths. The owner generation represents the bus unique-name arrival, not the presence of one particular item path.
- Cause: `src/shell/status_notifier/item_client/src/status_notifier_item_monitor.cpp:199-225` deletes the last `ItemSlot` on an item-unregistered signal but correctly leaves the registry owner live. `watchItemOwner()` at `:388-407` recovers an existing generation only by scanning remaining item slots. With no slot left, a later path calls `beginOwnerGeneration()` again and rebases the still-live owner.
- Reproduction: one private watcher bus, one item connection with `/One` and `/Two`, no owner loss and no watcher transition. The watcher first advertises `/One`, then emits its item-unregistered signal, then advertises `/Two`:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ./build/tray_candidate_repros \
  oneOwnerGenerationSurvivesLastPathRetirement -v1
```

Run from `/home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/repros`; exit **1**. Observed `owner generation before=1 after=2`. Expected the generation to remain `1` until that unique bus owner departs or a fresh watcher epoch begins. The registered simultaneous-two-path row passes but does not cover this last-path-retirement sequence.

### P2

None.

### P3

#### P3-1 — Decoder prose gives contradictory wrong-type policy

`src/shell/status_notifier/item_client/src/status_notifier_item_client.cpp:22-25` still says every wrong top-level property type is ignored as absent, while the repaired presentation fields now reject wrong types. Conversely, `docs/wiki/shell/status-tray.md:154-156` and the public header say every recognized wrong type fails the descriptor, while recorded-only `OverlayIconName`, `ItemIsMenu`, `Menu`, and `WindowId` are deliberately safe-dropped at `status_notifier_item_client.cpp:296-316`. The implementation is bounded, but the comments should distinguish presentation facts from recorded-only optional facts.

## Prior-finding closure recheck

The repaired assertions are registered in the normal seven-row tray suite. Inspection against `9d1a30b` confirms that each assertion is non-vacuous and would reject the old behavior:

| Prior item | Descendant control and observed result | Why it fails on `9d1a30b` |
| --- | --- | --- |
| P1-1 host retirement signal | `emitsHostUnregisteredWhenOwnerDisconnects`: payload-free wire count 1, local owner signal count 1 | Old `retireHost()` emitted neither wire nor local unregistered signal. The installed `/usr/share/dbus-1/interfaces/kf6_org.kde.StatusNotifierWatcher.xml:36-40` confirms both host signals have no payload, so the implementer's rebuttal to the prior `QString` receiver is accepted. |
| P1-2 signed action signature | `dispatchesRecordedIntents`: strict fake has `(int,int)` and records all four intents | Old client and fake used `(int,uint)` for the second coordinate. |
| P1-3 two paths / one generation | `populatesTwoPathsFromOneOwner`: count 2, population complete, generations equal | Old `watchItemOwner()` called `beginOwnerGeneration()` for every path, leaving one item and wedged population. |
| P1-4 root path | `populatesRootObjectPath`: count 1 and path `/` | Old parser rejected a separator at the final byte. |
| P1-5 wrong string types | `rejectsWrongTypedRequiredStrings`: invalid result; identity/title remain empty | Old generic `toString()` accepted integer `Id` and `Title`. |
| P1-6 timeout proof | `reportsLiveOwnerTimeouts`: a live object receives one call, withholds the reply, elapsed assertion `>=120 ms`, status `TimedOut`; `reportsImmediateTransportErrors` separately asserts `TransportError` | Old row addressed a nonexistent owner, returned immediately, and exposed only the shared `replyReceived=false` boolean. |
| P1-7 theme-root escape | `locatorRejectsEscapingThemeDirectories`: hostile `../outside` lookup is empty | Old locator returned the outside file. Canonical containment also closes symlink resolution by inspection. |
| P1-8 themed/fallback dimensions | `rendererBoundsThemeAndFallbackImages`: oversized themed input falls back and a 513 request yields 512×512 | Old decoder returned 513×1 and fallback allocated 513×513. |
| P2-1 degraded start idempotence | `degradedStartIsIdempotent`: both calls return true and state remains `NameOwnedElsewhere` | Old repeated call returned false outside `Active`. |
| P3-1 stale future-adapter prose | `productionProtocolCommentsStayCurrent` passes | The old limits header contains both forbidden future-only phrases, so the source-policy assertion is not tautological. |

The protocol-correct focused controls all exited 0:

```text
watcher controls: 4 passed, 0 failed
item-client controls: 6 passed, 0 failed
monitor controls: 4 passed, 0 failed
icon controls: 4 passed, 0 failed
values source-policy control: 3 passed, 0 failed
```

The non-vacuous timeout row was also timed independently:

```sh
TIMEFORMAT='ELAPSED=%R'; time env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/debug/tests/shell/status_notifier/qindaqt_status_notifier_item_client_tests \
  reportsLiveOwnerTimeouts -v1
```

Exit 0; 3/3 QTest functions passed; `ELAPSED=0.212`, compared with the configured 150 ms deadline and the row's `>=120 ms` assertion.

The prior scratch controls were rebuilt against the descendant. The prior host receiver was corrected from `QString` to the authoritative payload-free signature, and the traversal assertion was corrected to accept the safe empty result explicitly stated as expected in the first verdict. The combined descendant run exited 0 with 12 passed, 0 failed, 0 skipped. Observed: host-unregistered count 1; signed activation count 1; two-path count 2 with complete population; root count 1; wrong-typed snapshot rejected; escape lookup empty; oversized themed render fell back to 16×16; oversized placeholder clamped to 512×512; repeated degraded start returned true.

## Regression and boundary review

- Ownership truth/no takeover: the watcher still refuses to claim `org.kde.StatusNotifierWatcher` when another connection owns it, remains `NameOwnedElsewhere`, and refuses registrations. Its private-bus row passes in both profiles.
- Bounded item facts: existing hostile text, control-character, tooltip, pixmap count/dimension/byte, malformed shape, and last-known-good registry rows all pass. The repaired exact-type checks close the prior integer-to-string coercion.
- DBusMenu reuse: this branch is linear from `9d1a30b`; it did not merge a dbusmenu transport from `main`. It records only `ItemWireDetails::menuObjectPath`; no duplicate dbusmenu parser/client or cross-module private dependency was introduced. Rendering remains explicitly deferred.
- Host impact: only per-test private `dbus-daemon --session` fixtures were used. `DBUS_SESSION_BUS_ADDRESS` was unset and `DBUS_SYSTEM_BUS_ADDRESS` pointed to `/nonexistent`; no `tests/session`, nested compositor, host bus service, hardware, uinput, or network row ran.
- Shared registries/build files: the repair adds only a compile definition to the existing tray values test target. No JSON changed.

## Exact build and test evidence

### Identity and diff inspection

```sh
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git merge-base 9d1a30b HEAD
git status --porcelain
git diff --find-renames --find-copies 9d1a30b..HEAD
```

Results: exact candidate/tree/parent as recorded above; merge base `9d1a30b02729c0aba6deba9a43fa67418d283e76`; status empty. The graph is linear (`9d1a30b` → `8e44df5d` → `f4a6cce7` → `b66abc08` → `24a27d66` → `4c8e47b`), so there are no merge resolutions to audit.

### Configure

Both exact prescribed commands exited 0. CMake printed the repository's existing mixed Qt search-path warnings.

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

### Focused builds

The first parallel invocation was still compiling when the 30-second tool yield occurred. The same complete target command was immediately rerun for each profile and exited 0.

```sh
cmake --build <ROOT>/<profile> --parallel 3 --target \
  qindaqt_status_notifier qindaqt_status_notifier_watcher \
  qindaqt_status_notifier_item_client qindaqt_status_notifier_icon \
  qindaqt_status_notifier_watcher_tests qindaqt_status_notifier_item_client_tests \
  qindaqt_status_notifier_monitor_tests qindaqt_status_notifier_icon_tests \
  qindaqt_status_notifier_registry_tests qindaqt_status_notifier_presentation_tests \
  qindaqt_status_notifier_values_tests
```

Debug exit 0; Release exit 0.

### Discovery and selectors

The literal requested discovery command exited 0, but because the build-root path itself contains `tray`, its grep also matched CTest's unrelated missing-executable diagnostics for targets not built in this focused review:

```sh
ctest --test-dir <ROOT>/<profile> -N | grep -i -E 'status-notifier|tray'
```

The equivalent test-name-only refinement exited 0 and listed exactly seven rows, tests 151–157, in each profile:

```sh
ctest --test-dir <ROOT>/<profile> -N 2>/dev/null \
  | grep -i -E '^  Test +#[0-9]+: .*(status-notifier|tray)'
```

Rows: values, registry, presentation, watcher, item-client, monitor, icon.

Both exact tray selectors exited 0, with 7/7 rows passed, 0 failed, 0 skipped:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 \
  ctest --test-dir <ROOT>/<profile> -R '^qindaqt\.status-notifier-' \
  --output-on-failure --no-tests=error
```

Independent per-binary totals in both Debug and Release were identical: values 18, registry 25, presentation 9, watcher 13, item-client 14, monitor 10, icon 13 = **102 passed, 0 failed, 0 skipped** per profile. There are no tray QML rows; `QT_FATAL_WARNINGS=1` was set for the complete selector anyway.

### Scratch harness

```sh
cmake -S /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/repros \
  -B /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/repros/build \
  -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/repros/build --parallel 3
```

Both exited 0. The harness links the exact candidate's Debug libraries and lives entirely beneath the assigned build root.

### Static gates

- `./tools/validate-docs` — exit 0; 116 Markdown documents and `mkdocs.yml` navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-tray-codex/site` — exit 0.
- `./tools/check-source-shape` — exit 0; 1,778 files checked. It printed only the two unrelated pre-existing decomposition warnings: `tests/compositor/CMakeLists.txt` at 500 nonblank lines and `tests/services/display_color_model/tst_color_model.cpp` at 539. The repaired item-client implementation is 495 nonblank lines.
- `git diff --check` — exit 0.
- `git diff --check 9d1a30b..HEAD` — exit 0.
- `git diff --name-only 9d1a30b..HEAD -- '*.json'` — exit 0 with empty output; `python3 -m json.tool` is not applicable because no JSON changed.

## Final tree integrity

```text
git rev-parse HEAD        = 4c8e47b28d2d92711bf433c8d2a5afc9be59030f
git rev-parse HEAD^{tree} = 47cd0c50922b468f94e1374a287a8c4def526791
git rev-parse HEAD^       = 24a27d666085b601135ac6632e5a0ef5f4be1aa9
git rev-parse 9d1a30b     = 9d1a30b02729c0aba6deba9a43fa67418d283e76
git rev-parse ce9228d     = ce9228d9694622d503d92a38d01986f8f124f188
git status --porcelain    = empty
```

VERDICT REJECT P0/P1/P2/P3=0/1/0/1
