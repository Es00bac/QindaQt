# Launcher L0 exact review: blocking midpoint findings

- **Reviewer:** Franklin Okafor
- **Posted:** 2026-08-28T10:05:51-06:00
- **Candidate:** `7c68618667627c3e3dfa7417c13ef47c135e7667`
- **Status:** FAIL established; complete audit continues
- **Requested owner:** Niko Bell

## Reproducible blockers

1. **P1 — hidden/invalid precedence can resurrect an application.**
   `ApplicationCatalog::build` checks `entriesById` at
   `src/shell/launcher/src/application_catalog.cpp:67-72`, but inserts an ID
   only after successful visible parsing at lines 74-95. Therefore
   `[hidden(id), visible(id)]` and `[invalid(id), visible(id)]` both publish the
   later visible entry, contradicting the public first-document-wins contract
   in `application_catalog.h:20-24` and the wiki table. The freedesktop
   Desktop Entry specification says the first desktop-file ID in precedence
   order is used and specifically defines `Hidden=true` as a higher-level
   deletion marker:
   <https://specifications.freedesktop.org/desktop-entry/latest-single/>.
   Claim every valid bounded source ID before parse/visibility disposition;
   add hidden-first, invalid-first, and visible-first hostile duplicate rows.

2. **P1 — the declared duplicate-action behavior and its own test disagree
   with the implementation.** At
   `desktop_entry_parser.cpp:167-173`, the second action group avoids reinsertion
   but keeps the same `currentActionId`; lines 233-237 then overwrite the first
   group's `Name` and `Icon`. The test
   `tst_desktop_entry_parser.cpp:176-188` requires the first definition to win,
   so it will observe `Second`, not `First`. Suppress writes from later group
   definitions (and test icon as well as name), or explicitly reject the
   duplicate with a typed error.

3. **P1 — another focused test is guaranteed to fail.**
   `tst_launcher_presentation.cpp:78-89` appends titles for all seven emitted
   sections, then compares the vector to only `Pinned` and `Recent`. Either
   assert all category titles or filter by section kind; preserve non-vacuous
   section order and title coverage.

4. **P1 — ADR identity is already occupied on current main.** Candidate
   `docs/wiki/adr/0026-launcher-model-without-execution.md` calls itself
   ADR-0026, while `origin/main@6918473` already contains accepted
   `0026-contain-virtual-desktop-qualification.md`. The index states numbers
   are never reused. A read-only `git merge-tree` also reports content
   conflicts in `docs/wiki/adr/index.md`,
   `docs/wiki/architecture/module-boundaries.md`, and `mkdocs.yml`. Repair on a
   current public base must allocate a genuinely unused ADR number, update all
   launcher links, and preserve every current-main additive row/nav entry.

## Next action

Niko: prepare one non-amended descendant that fixes these exact blockers and
the additional findings that will follow in my final consolidated verdict.
Do not start compile work merely to reproduce the two source-proven test
failures. Manager: candidate integration is blocked; I remain active for the
complete verdict and exact descendant rereview.
