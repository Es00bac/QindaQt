# Display D1 repair source-ready: Mina P0 closed, waiting for serial qualification

- **Timestamp:** 2026-08-28T04:26:20Z
- **From:** Kellan Ward, Display D1 transaction implementer and lead (`/root/display_d1`)
- **To:** QindaQt manager, Mina Shah, Iris Hale, and Elara Finch
- **Worktree/branch:** `/home/cabewse/work_SPaC3/container-wm-workers/display-d1`, `worker/display-d1`
- **HEAD/tree:** `0e38fa726af69e34be3cacdd6b71d40350ac8092` / `53880d210952cccb0a44f7dd46fbcc9bac22a8f5`
- **Merge base:** exact public base `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- **State:** source/static complete; waiting, not live; Controls retains the sole compiler/private-runtime lane
- **Commit:** none; failed candidate was not amended

## Mina P0 closure

The full public include graph confirms Mina's `1787889908` trace. The four
production translation units include `transaction_machine.h` or
`transaction_journal.h` before the later private/source headers that happened
to include `display_limits.h`; `display_types.h` itself includes only Qt Core
types. The repaired `transaction_types.h`, which directly names
`Display::kMaximumRevertAttempts`, now directly includes its owning public
`display_limits.h` before `display_types.h`.

This is the only change since checkpoint `1787882078`: one insertion in an
already tracked path. It preserves the existing permitted dependency direction
display_transaction → display_protocol and makes every public consumer
independent of include order. No broad include cleanup or dependency change was
made.

I reread Iris's entire `1787889831` verdict and Mina's entire `1787889908`
handoff after the fix, then read the complete resulting diff for every changed
path. Iris's verified P1/P2/P3 behavior remains byte-for-byte unchanged except
for the required include; Mina's sole P0 is directly closed. No accidental
product, test, documentation, registry, or worker-record change was found.

## Exact source/static evidence

- `git diff --check`: exit 0.
- `./tools/validate-docs`: exit 0; 51 Markdown documents and `mkdocs.yml`
  navigation validated.
- `./tools/check-source-shape --largest 30`: exit 0; 885 source files, zero
  allowlist skips. Largest touched Display production source is
  `transaction_machine_events.cpp` at 462 non-blank lines; largest touched
  Display test is `tst_transaction_adversarial.cpp` at 466.
- Corrected array-based forbidden dependency/runtime scan over all four owned
  production modules: exit 0. No QObject/timer/real clock, settings/filesystem,
  KWin/Wayland/QML/KScreen/logind, D-Bus connection/service runtime, or other
  forbidden provider symbol; Qt DBus remains confined to protocol
  serialization. Link boundaries remain identity=Qt Core,
  protocol=Qt Core+Qt DBus, topology=protocol+Qt Core, and
  transaction=protocol+topology+Qt Core.
- Host-path/shell-diagnostic sweep across touched docs/source/tests: exit 0,
  no match. Tracked worker-record sweep: exit 0, no match.
- Current tracked diff: exactly 15 paths, 245 insertions, 26 deletions. HEAD and
  tree remain exactly the failed immutable candidate above; merge base remains
  the exact public base. The sole external untracked path is still
  `ops/team/workers/kai-mercer.md` and is excluded from any future candidate.

No configure, compile, test binary, install/package, display/session, runtime,
or host-state command ran. Static scripts above are the only executed gates.

## Future serial qualification sequence after explicit lane assignment

All compiler work will use one job and stop on first failure:

1. Reconfirm exact HEAD/diff and claim the released lane; compile the four
   Display libraries and all eleven focused test executables in strict Debug.
2. Run `^qindaqt\\.display-` serially (expected discovery: 11 selectors), then
   complete the broad Debug build and broad CTest suite, recording exact counts
   and separating any ambient/base failure from this repair.
3. Use a fresh strict Release tree; repeat focused build/test followed by the
   proportional broad Release build/test.
4. Use a fresh ASan+UBSan Debug tree with frame pointers, leak detection, and
   halt-on-error; build and run all eleven Display selectors serially.
5. Stage the Release install into a new private temporary prefix, verify the
   exact four Display libraries, fifteen public Display headers, and exported
   dependency metadata, then build a clean installed-package consumer whose
   first QindaQt include is `transaction_types.h` and which references the
   shared revert limit. This directly qualifies Mina's include-order P0 at the
   package boundary.
6. Rerun strict MkDocs, docs/navigation, source-shape, forbidden dependency,
   and whitespace/diff gates. Exclude the external worker record, create a new
   non-amended evidence-rich repair commit only after every gate is clean, and
   send that exact immutable SHA to Elara for bounded rereview.

Requested next action: manager assignment of the serial compiler/private
package lane after Controls reaches terminal and releases it. Until then this
worktree is preserved and Kellan is waiting/not live.
