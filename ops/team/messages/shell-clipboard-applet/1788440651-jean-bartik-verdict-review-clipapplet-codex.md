# Jean Bartik — independent Clipboard applet C1 exact-candidate review

- Persona: **Jean Bartik**, independent shell-applet reviewer
- Provider/model: **OpenAI Codex `gpt-5.6-sol`**, reasoning high
- Exact candidate SHA: `759c639bc3978644447f78b3d223830581890d6c`
- Tree SHA: `1946d264f48691e7f9827bdb35153f480f6fc555`
- Parent SHA: `766d629cf1b42e930bdec57c02e6d6a687e818c5`
- Base SHA: `74da46345c7a5094d45c756ad8b23ca87591fcd3`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/clipboard-applet-c1-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex`
- Initial and final worktree state: exact candidate checked out; `git status --porcelain` empty

## Findings ledger

### P0

None.

### P1

#### P1-1 — The injected seam accepts and rediscloses hostile, stale, and foreign-owner snapshots

The controller assigns every incoming `HistorySnapshot` without validating its
descriptor floor, collection bound, generation/revision monotonicity, or owner
lineage at `src/shell/clipboard_applet/src/clipboard_applet_controller.cpp:227`–253.
`onStateChanged()` merely reprojects at lines 222–225; it does not clear the
accepted baseline on owner loss/replacement. The pure projector then copies
labels/previews and counts the whole untrusted collection at
`src/shell/clipboard_applet/src/clipboard_applet_model.cpp:76`–119 and 173–215.

Reproduction:

```sh
/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/controller_hostile
```

Exit 1. Relevant observed output:

```text
stale snapshot: phase= ready entries= 1 preview= stale-generation-secret
hostile metadata: entries= 1 sourceLength= 4098 previewLength= 4097 sourceHasNewline= true previewHasFormatChar= true sourceUtf8RoundTrips= false
forged sensitive descriptor: entries= 1 mediaType= application/x-qindaqt-secret preview= one-time secret
oversized collection: inputEntries= 5000 projectedRows= 32 reportedUnpinned= 5000
owner replacement without baseline: phase= ready entries= 1 preview= owner-A-secret
```

Expected: a generation below the accepted high-water, a snapshot from a new
owner without a fresh baseline, any descriptor that fails the C0 descriptor
floor (including sensitive media, unpaired UTF-16, control/format characters,
or overlong fields), and any collection above the C0 limit must fail closed
without presenting content. Observed: all are accepted and projected; only the
last list rendering step caps 5,000 entries to 32 rows.

This is also a test-quality defect in the only claimed hostile proof:
`tests/shell/clipboard_applet/tst_clipboard_applet_model.cpp:334`–385 explicitly
projects invalid IDs, negative byte counts, control characters, and bidi format
characters and merely asserts that output is nonempty. The named owner-fencing
test at `tests/shell/clipboard_applet/tst_clipboard_applet_controller.cpp:159`–182
starts with no entry, so it cannot detect owner-A content reappearing under
owner B.

#### P1-2 — Unlock overrides an independent privacy denial that arrives while locked

`ClipboardModelClientAdapter::setLocked()` records only a Boolean
`m_privacyDeniedByLock`; if lock first changes Allowed to Denied, a separate host
denial while that Denied state is active is indistinguishable. Unlock therefore
unconditionally restores Allowed at
`src/shell/clipboard_applet/src/clipboard_model_client_adapter.cpp:68`–77,
contradicting the adjacent AGENT-GUARD and the wiki promise that independent
host denial survives unlock.

The same reproduction exits 1 and reports:

```text
overlapping independent denial after unlock: privacyAllowed= true expected=false
```

Expected: privacy remains denied after unlock because an independent denial is
still active. Observed: unlock grants privacy. The candidate test covers only a
denial that predates lock, not an overlapping denial that arrives during lock.

#### P1-3 — The Pin button forwards `undefined`, not the entry serial

At `src/shell/clipboard_applet/qml/ClipboardEntryRow.qml:168`, the Pin handler
calls `togglePin(rowRoot.entry.generation, rowRoot.serial)`. `rowRoot` has no
`serial` property; the intended value is `rowRoot.entry.serial`.

Reproduction under the requested fatal-warning environment:

```sh
env QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1 QML2_IMPORT_PATH=/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/qml /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/tests/shell/clipboard_applet/qindaqt_clipboard_applet_qml_tests -input /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/tst_ClipboardAppletReview.qml
```

Exit 2, 2 passed / 2 failed:

```text
FAIL! ... test_pinForwardsEntrySerial()
Actual   (): undefined
Expected (): 73
```

Expected: clicking Pin dispatches the displayed entry's `(generation, serial)`.
Observed: the serial is `undefined`; a real typed C++ controller cannot receive
the intended ID. The candidate's interactive row checks only that the fake
method call count increments (`tst_ClipboardAppletInteractive.qml:159`–164), so
its argument assertion is vacuous.

#### P1-4 — Write denial incorrectly disables metadata search

The capability contract says `clipboard.write` denial keeps browsing **and
search** live, and the C++ controller test demonstrates that read-only search
works. QML instead binds the search field to `root.actionsEnabled`, which
includes the write grant, at
`src/shell/clipboard_applet/qml/ClipboardApplet.qml:30`–31 and 76–84.

The same offscreen reproduction reports:

```text
FAIL! ... test_readOnlyGrantKeepsSearchEnabled()
Actual   (): false
Expected (): true
```

Expected: `clipboard.read=true`, `clipboard.write=false`, Ready keeps metadata
search enabled while mutation controls remain disabled. Observed: the search
field is disabled.

### P2

#### P2-1 — A mismatched completion ID strands an entry's pending marker

The controller looks up the trusted pending request by `requestId`, erases that
request, but removes the row marker using the untrusted `outcome.id` instead of
the stored `PendingRequest::id` at
`src/shell/clipboard_applet/src/clipboard_applet_controller.cpp:291`–306.

Reproduction (`controller_hostile`, exit 1):

```text
mismatched completion: pendingRequests= 0 retryAccepted= false
```

Expected: a mismatched completion is rejected or the marker for the request's
recorded entry is cleared. Observed: no request remains, but the initiating
entry remains permanently pending and a retry is refused.

#### P2-2 — A valid maximum observed tick wraps the next promote tick to zero

`noteObservedTicks()` accepts `quint64` tick values through the full declared
range at `clipboard_applet_controller.cpp:312`–319; `selectEntry()` then uses
`++m_nextPromoteTick` at line 398 with no exhaustion check.

Reproduction (`controller_hostile`, exit 1):

```text
max observed tick: dispatchedTick= 0 expectedGreaterThan= 18446744073709551615
```

Expected: fail closed at exhaustion (or use an explicit non-wrapping policy).
Observed: the supposedly strictly monotonic tick wraps to zero.

#### P2-3 — The installed-package row does not prove relocation

The consumer embeds the staging prefix in absolute `BUILD_RPATH` and in
compile-time QML/theme paths at
`tests/shell/clipboard_applet/installed_consumer/CMakeLists.txt:33`–47. The
test runs the consumer at that same original prefix with an explicit
`LD_LIBRARY_PATH`; it never relocates the stage. This does not support the
relocation claim in `docs/wiki/shell/clipboard-applet.md:116`–120.

Observed RUNPATH:

```sh
readelf -d /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/tests/shell/clipboard_applet/installed-clipboard-applet-stage/consumer-build/qindaqt_installed_clipboard_applet_consumer | rg 'RPATH|RUNPATH|NEEDED'
```

Exit 0; RUNPATH contains the full original
`.../debug/tests/shell/clipboard_applet/installed-clipboard-applet-stage/...`
paths.

Actual relocation reproduction (the stage was restored afterward):

```sh
test ! -e /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/relocated-stage && mv /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/tests/shell/clipboard_applet/installed-clipboard-applet-stage /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/relocated-stage
env -u LD_LIBRARY_PATH QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/relocated-stage/consumer-build/qindaqt_installed_clipboard_applet_consumer
mv /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/relocated-stage /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug/tests/shell/clipboard_applet/installed-clipboard-applet-stage
```

Consumer exit 127:

```text
error while loading shared libraries: libqindaqt_controls_qml.so: cannot open shared object file: No such file or directory
```

Expected: a row claiming relocation must move the staged prefix and still
resolve/run from staged files. Observed: moving it breaks dynamic resolution.

#### P2-4 — The keyboard/accessibility rows do not cover the promised interactive surface

The accessibility row asserts only the root grouping, one list row, and the
search field (`tst_ClipboardAppletAccessibility.qml:80`–109). It never checks
roles, names, descriptions, or enabled/busy state for Pin, Delete, either Clear
button, or feedback dismissal. The keyboard row contains only direct Return and
Delete delivery to an already-focused row (`tst_ClipboardAppletKeyboard.qml:97`–115);
it performs no Tab/Backtab traversal and no keyboard activation of Pin, Delete,
search-clear, history-clear, or feedback-dismiss controls.

Reproduction:

```sh
rg -n 'pinButton|deleteButton|clearAllButton|clearUnpinnedButton|feedbackState' tests/shell/clipboard_applet/qml/tst_ClipboardAppletAccessibility.qml
rg -n 'function test_' tests/shell/clipboard_applet/qml/tst_ClipboardAppletKeyboard.qml
```

First command exits 1 with no matches. The second exits 0 and lists only
`test_rowKeyboardReturnKey` and `test_rowKeyboardDeleteKey`. Expected: explicit
coverage for every interactive element and actual focus traversal, as required
by the review brief and claimed by `clipboard-applet.md:135`–136.

### P3

#### P3-1 — The public injected interface does not state its threading/error/lifetime contract

`clipboard_client_interface.h:12`–47 documents the authority shape and unique
request IDs, but not whether direct virtual calls and signals must remain on the
GUI thread, how queued cross-thread delivery is handled, what happens at client
destruction, whether zero is valid, or the required outcome lineage. The owning
wiki says the controller borrows the seam, but a later real Clipboard1 client
cannot bind safely without these public requirements. This is precision debt
under the root public-interface rule; it is not the basis of the rejection.

## Review-question answers

1. **Privacy — fail.** No payload-byte member exists in the applet projection,
   and the in-process lock path purges the normal C0 fixture, but the seam does
   not validate snapshots or fence owner/generation/revision. It presents
   overlong/control/invalid-Unicode metadata, forged sensitive media, 5,000-entry
   collections, stale generations, and old-owner content. The overlapping
   privacy-denial sequence also re-grants privacy on unlock (P1-1/P1-2).
2. **Least authority — partial, but fail overall.** Controller mutation entry
   points call `refuseMutation()` before dispatch and the C++ read/write-grant
   tests pass. Intents go through the injected seam and are not executed by QML
   or the controller. However QML sends an invalid Pin identity, wrongly makes
   read-only search require write authority, incoming owner/generation fencing
   is absent, and a mismatched result can strand pending state (P1-1/P1-3/P1-4/P2-1).
3. **Integrity — registration passes; relocation claim fails.** Manifest,
   policy, registry, root/source/test CMake edits are additive. Manifest/catalog/
   resolver/host rows pass 6/6 in each profile, and the focused installed row
   passes in its original stage. The RPATH probe does not relocate the stage and
   the consumer fails after an actual move (P2-3).
4. **Tests non-vacuous — mixed.** Literal base `74da463` contains neither the
   applet nor the fencing target, so the suite cannot be run on that pre-candidate
   tree. `git cat-file -e 74da463:tests/shell/clipboard_applet/tst_clipboard_applet_fencing.cpp`
   exits 128. An independent drain-before-register mutation does demonstrate
   that the synchronous reply ordering check is non-vacuous: candidate ordering
   exits 0 with result count 1; the mutation exits 1 with result count 0.
   Conversely, the Pin test ignores its arguments, hostile descriptor tests
   approve invalid output, owner fencing starts empty, and keyboard/accessibility
   coverage is incomplete (P1-1/P1-3/P2-4).
5. **Boundaries/docs — source boundary passes, claims do not.** The diff contains
   no `src/shell/runtime/**`, `src/shell/qml/**`, Wayland, D-Bus, or Clipboard
   service implementation edit. The boundary poison and source-shape gates pass,
   as do docs/link checks. The wiki nevertheless overclaims hostile/owner fencing,
   complete keyboard/accessibility evidence, and relocation (findings above).

## Commands and executed evidence

### Identity and cleanliness

```sh
git rev-parse HEAD
git status --porcelain
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git rev-parse 74da463
```

Both before and after review: candidate/tree/parent/base match the header;
status output empty.

### Configure

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/release -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Debug exit 0; Release exit 0. CMake emitted existing dependency-path warnings,
but generated both build trees successfully.

### Focused builds

The following command was run once with `<PROFILE>=debug` and once with
`<PROFILE>=release`:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/<PROFILE> --parallel 3 --target qindaqt_shell_clipboard_applet qindaqt_shell_clipboard_applet_runtime qindaqt_shell_clipboard_applet_runtimeplugin qindaqt_clipboard_applet_model_tests qindaqt_clipboard_applet_controller_tests qindaqt_clipboard_applet_fencing_tests qindaqt_clipboard_applet_seam_tests qindaqt_clipboard_applet_qml_tests qindaqt_applet_manifest_tests qindaqt_applet_catalog_tests qindaqt_applet_instance_resolver_tests qindaqt_applet_host_policy_tests qindaqt_applet_host_handshake_tests qindaqt_applet_host_lifecycle_tests qindaqt_clipboard_media_tests qindaqt_clipboard_history_tests qindaqt_clipboard_history_lineage_tests qindaqt_clipboard_codec_tests
```

Debug: exit 0, 250/250 build steps. Release: exit 0, 250/250 build steps.

### Debug tests

```sh
env QT_FATAL_WARNINGS=1 ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug -R '^qindaqt\.clipboard-applet-' --output-on-failure --no-tests=error
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug -R '^qindaqt\.(applet-manifest|applet-catalog|applet-runtime|applet-host)' --output-on-failure --no-tests=error
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug -R '^qindaqt\.clipboard-model-' --output-on-failure --no-tests=error
```

- Focused Clipboard applet: exit 0, **10/10** passed; 4/4 offscreen rows ran
  under `QT_FATAL_WARNINGS=1`; installed package row passed.
- Adjacent manifest/catalog/resolver/host: exit 0, **6/6** passed.
- C0 Clipboard model: exit 0, **4/4** passed.

### Release tests

```sh
env QT_FATAL_WARNINGS=1 ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/release -R '^qindaqt\.clipboard-applet-' --output-on-failure --no-tests=error
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/release -R '^qindaqt\.(applet-manifest|applet-catalog|applet-runtime|applet-host)' --output-on-failure --no-tests=error
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/release -R '^qindaqt\.clipboard-model-' --output-on-failure --no-tests=error
```

- Focused Clipboard applet: exit 0, **10/10** passed; 4/4 offscreen rows ran
  under `QT_FATAL_WARNINGS=1`; installed package row passed.
- Adjacent manifest/catalog/resolver/host: exit 0, **6/6** passed.
- C0 Clipboard model: exit 0, **4/4** passed.

### Static gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/site
./tools/check-source-shape
git diff --check
git diff --check 74da463..759c639
python3 -m json.tool data/applet-policy/default.json
python3 -m json.tool data/applets/clipboard.json
```

- `validate-docs`: exit 0, 117 Markdown documents/navigation validated.
- MkDocs strict build: exit 0.
- Source shape: exit 0, 1,785 files checked; only pre-existing warnings for
  `tests/compositor/CMakeLists.txt` (500) and
  `tests/services/display_color_model/tst_color_model.cpp` (539).
- Both diff checks: exit 0.
- Both JSON checks: exit 0.

### Synchronous-search negative control

Scratch sources and binaries are under the assigned build root in `repros/`.
The exact execution was:

```sh
/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/synchronous_search_baseline
/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/repros/synchronous_search_mutated
```

Candidate ordering: exit 0, `synchronous search result count= 1 expected=1`.
Drain-before-register mutation: exit 1,
`synchronous search result count= 0 expected=1`.

No nested/session/compositor rows, host D-Bus, hardware, uinput, or network were
run.

## Verdict

The candidate is rejected. Four P1 contract/functionality failures and four P2
correctness/evidence failures must be repaired and re-reviewed at a new exact
commit.

VERDICT REJECT P0/P1/P2/P3=0/4/4/1
