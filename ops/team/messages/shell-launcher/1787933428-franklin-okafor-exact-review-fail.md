# Launcher L0 exact-candidate review: FAIL

- **Reviewer:** Franklin Okafor (OpenAI collaboration runtime; exact serving
  model/reasoning unexposed)
- **Posted:** 2026-08-28T10:10:28-06:00
- **Candidate:** `7c68618667627c3e3dfa7417c13ef47c135e7667`
- **Tree:** `b2cdf38daed878087aed1e2045ea6e520830c5fc`
- **Parent:** `9db68c4023257b49421101fa1b13c73bbc2cfa85`
- **Verdict:** **FAIL; integration blocked**
- **Counts:** P0/P1/P2/P3 = `0/8/9/4`
- **Detached worktree:** clean at the immutable candidate
- **Requested next action:** Niko Bell prepares one non-amended repaired
  descendant; Franklin rereviews that exact commit

## P1 blocking findings

1. **No candidate-owned build/test route exists.** The advertised standalone
   paths in `tests/shell/launcher/CMakeLists.txt:12-23` use `../../cmake` and
   `../../src/shell/launcher`; from `tests/shell/launcher` those resolve to
   nonexistent `tests/cmake` and `tests/src/shell/launcher`. The repository
   root `src/CMakeLists.txt` and `tests/CMakeLists.txt` do not add Launcher
   either. Fix the standalone paths and current-main additive root wiring, then
   prove six registered non-vacuous rows.

2. **The pinned/recent suite cannot compile.** Its non-void helper
   `tests/shell/launcher/tst_launcher_pinned_recent.cpp:14-20` invokes
   `QCOMPARE`; QtTest's ordinary failure macro expands to a bare `return`, which
   is ill-formed in a function returning `PinnedApplications`. Return an
   explicit outcome or keep assertions in void test slots.

3. **Higher-precedence hidden/invalid entries do not claim their ID.**
   `application_catalog.cpp:67-95` inserts into `entriesById` only after a
   visible successful parse. A later same-ID visible document therefore
   resurrects a user-hidden/NoDisplay application and violates the public
   first-document-wins contract. The official Desktop Entry specification
   defines first-ID precedence and `Hidden=true` as a deletion marker:
   <https://specifications.freedesktop.org/desktop-entry/latest-single/>.
   Claim every bounded valid source ID before parse/visibility disposition;
   cover Hidden-first, NoDisplay-first, invalid-first, and visible-first.

4. **Repeated action groups overwrite the supposed first winner.**
   `desktop_entry_parser.cpp:167-173` preserves the hash entry but lines
   233-237 still write the second group's fields through the same action ID.
   The candidate's own `duplicateActionGroupKeepsFirstDefinition` test at
   `tst_desktop_entry_parser.cpp:176-188` will observe `Second`, not `First`.
   Reject duplicate group names as invalid or suppress every later write, with
   name and icon regressions.

5. **The ready-presentation assertion is internally impossible.**
   `tst_launcher_presentation.cpp:78-89` appends titles for all seven emitted
   sections and then compares the vector to only two titles. Assert all fixed
   category titles or filter the collection; preserve real section-order and
   label coverage.

6. **ADR-0026 is already accepted on current main.** Candidate
   `0026-launcher-model-without-execution.md` reuses an identifier occupied by
   `origin/main`'s accepted contained-virtual-desktop ADR, despite the index's
   never-reuse rule. Allocate a genuinely unused number and update every link.
   Preserve current-main AppShell, brightness/power, virtual-desktop, and Flow
   entries when resolving the three merge conflicts named below.

7. **Unknown keys/groups are not actually ignored.** Every value is unescaped
   at `desktop_entry_parser.cpp:205-209` before the parser decides whether the
   key/group belongs to the supported subset. An extension value containing an
   unowned escape rejects the entire document, contradicting the public/wiki
   forward-compatibility contract. Decide ownership before decoding and add
   hostile unknown-key and unknown-group rows.

8. **The accepted desktop-entry grammar is not correctly validated.** The
   official specification requires spaces around `=` to be ignored and
   permits escaped semicolons in list values; lines 186-206 reject the former
   and reject `\;` before list splitting. It also requires boolean values to be
   `true` or `false`, while `parseBoolValue` at lines 38-41 treats every invalid
   token as false, potentially exposing an invalid Hidden/NoDisplay entry.
   Implement typed per-value parsing and add whitespace, escaped-list, and bad-
   boolean regressions.

## P2 repair findings

1. `ApplicationCatalog::build` reserves `documents.size()` before enforcing
   `maxSourceDocuments` (`application_catalog.cpp:40-42`), and invalid
   over-ceiling source IDs are copied whole into diagnostics (60-65). Cap every
   retained allocation and diagnostic field at the public hostile boundary.
2. `PinnedApplications::pin` and `RecentApplications::record` accept and retain
   arbitrarily long identities despite `maxEntryIdLength`; add one shared
   identity validator and hostile boundary rows.
3. `Name` containing only whitespace is accepted, allowing visually and
   accessibly blank application rows; require a non-blank validated display
   name without silently changing the stored value.
4. Duplicate keys in one group are invalid per the desktop-entry grammar, but
   recognized fields currently use silent last-wins (Hidden alone uses OR).
   Return a typed duplicate-key/group failure consistently.
5. Action IDs are specified to follow key-name syntax and be unique, but the
   parser checks only length and will publish duplicate `Actions=` items.
   Validate `A-Za-z0-9-`, reject/deduplicate repeated declarations, and cover
   empty/malformed IDs.
6. A valid no-match query emits zero sections
   (`launcher_presentation.cpp:92-101`), contradicting the wiki/API promise
   that search collapses to one SearchResults section and leaving the adapter
   without the promised empty-results container.
7. `makeItem` copies only `comment` into `accessibleDescription`; comments are
   optional, so many rows have an empty accessible description despite the
   accessibility-ready contract. Define and test a deterministic non-empty
   fallback.
8. User-visible section titles are hard-coded English inside the pure Qt Core
   policy model (`launcher_presentation.cpp:23-51`). Project enum/identity from
   the model and localize in the future presentation adapter, or introduce an
   explicit translation boundary without making locale affect ordering.
9. `keyboardAndPointerActivationShareOneIntentPath` calls the exact same
   catalog method twice (`tst_launcher_presentation.cpp:196-209`) and therefore
   proves no independent input path. Keep the architectural rule in the ADR,
   but do not count this row as interaction evidence; add real adapter-path
   evidence when those paths exist. The repaired L0 tests must instead cover
   intent confinement and action/icon fallback directly.

## P3 hardening findings

1. `maxDocumentBytes` is enforced with `QString::size()` (UTF-16 code units),
   and the error itself calls it a code-unit ceiling. Rename the limit/contract
   or accept bounded bytes before UTF-8 decoding so producer and consumer units
   cannot drift.
2. The `launcher_category_model.cpp:16-19` AGENT-GUARD says mapping-table row
   order controls first match, but `categoryFor` loops input categories first;
   input order controls the winner. Correct the future-agent guard.
3. `DiagnosticKind::HiddenEntrySkipped` exists but the documented behavior is
   deliberately silent and no code emits it. Remove the misleading dead
   contract or document a future use.
4. `launcher_search_ranker.cpp` uses `std::sort` without directly including
   `<algorithm>`; do not rely on Qt transitive includes.

## Verification and integration evidence

- Exact tuple and parent: pass; detached tree remained clean.
- Changed-path manifest: 29 paths, 2,804 insertions; no unexpected paths.
- `git diff --check 9db68c4..7c68618`: exit 0.
- `python3 tools/check-source-shape`: exit 0; 1,025 files checked, largest
  candidate production source 272 non-blank lines.
- `python3 tools/validate-docs`: exit 0; 65 Markdown documents and navigation.
- `mkdocs build --strict`: exit 0 in an ignored private build directory.
- `git merge-tree --write-tree 7c68618 origin/main`: exit 1; content conflicts
  in `docs/wiki/adr/index.md`, `docs/wiki/architecture/module-boundaries.md`,
  and `mkdocs.yml`; `docs/wiki/index.md` auto-merges. The ADR identity collision
  remains semantic even after textual resolution.
- Configure/compile/CTest: **not run** because the manager did not release the
  single compiler lane. No pass is claimed. Source inspection proves the
  standalone configure path and one compilation unit fail before execution,
  while two additional candidate assertions contradict their implementation.
- No host GUI, desktop session, bus, input, or configuration was touched.

## Repair acceptance

Publish one clean, non-amended descendant with the exact tuple and path
manifest; a current-main-safe unique ADR; correct source/root/test wiring;
strict serial compilation; six focused CTest rows; hostile parser/catalog,
bounds, search, pinned/recent, no-result, accessibility, and intent regressions;
source-shape/docs/MkDocs/diff gates; and a fresh merge-tree collision report.
Request Franklin's exact descendant rereview before manager integration.
