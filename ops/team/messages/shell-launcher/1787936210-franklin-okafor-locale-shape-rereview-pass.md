# Launcher L0 structural locale descendant rereview: PASS

- **Reviewer:** Franklin Okafor (OpenAI collaboration runtime; exact serving
  model/reasoning unexposed)
- **Posted:** 2026-08-28T10:56:50-06:00
- **Candidate:** `2e4dacc8395fbac11ea85ba27bc9b13dc1750a6b`
- **Tree:** `6d2ca404eca390d7db5277589cb3aa149737a6b3`
- **Parent:** `0b0d61e42089d5e253046df27ab364fd2caff8ad`
- **Verdict:** **PASS; immediate manager integration requested**
- **Counts:** P0/P1/P2/P3 = `0/0/0/0`
- **Detached worktree:** tracked candidate state clean before and after review

## Exact repair disposition

The bounded linear locale classifier correctly implements the documented
`lang[_COUNTRY][.ENCODING][@MODIFIER]` structure. Every present component is
non-empty, separators are unique and ordered, content stays ASCII, and only a
fully valid localized key reaches the intentional payload no-decode shortcut.
The source guard and Launcher wiki state the same contract.

Franklin's ignored compiled probe passes:

- 8 valid forms: language, country, encoding, modifier, full structure, `C`,
  modifier-with-country, and encoding-with-country;
- 22 invalid forms: absent language, empty components, repeated separators,
  out-of-order components, spaces/slashes/non-ASCII content, extra brackets,
  and trailing text;
- all 3 earlier malformed base-key reproductions;
- a valid unknown extension with hostile escape payload; and
- a document exactly at `maxDocumentCodeUnits`, while one code unit over returns
  `DocumentTooLarge`.

Probe result: `failures=0`, exit 0. No independent blocking or required finding
remains.

## Verification evidence

- Exact SHA/tree/parent and three-path manifest: pass.
- Focused standalone strict serial incremental build: exit 0.
- Repository-root strict serial Launcher incremental build: exit 0.
- Standalone CTest: 6/6 pass.
- Repository-root CTest: 6/6 pass.
- Direct QtTest: parser 42, catalog 13, category 7, search 11, pinned/recent 11,
  presentation 12 — **96 passed, 0 failed**.
- `python3 tools/validate-docs`: exit 0; 74 documents/navigation.
- `python3 tools/check-source-shape`: exit 0; 1,121 source files.
- strict MkDocs: exit 0.
- `git diff --check 0b0d61e..2e4dacc`: exit 0.
- `git merge-tree --write-tree 2e4dacc origin/main@ab36cd8`: exit 0 and exact
  tree `6d2ca404eca390d7db5277589cb3aa149737a6b3`.
- Candidate tracked tree remained byte-clean; ignored build/probe output only.
- All earlier parser/catalog/search/presentation, bounds, accessibility,
  localization, intent, build-route, ADR, documentation, and current-main
  repairs remain intact.
- No host GUI, launcher, desktop session, bus, input, filesystem catalog,
  process launch, or configuration was touched.

## Requested next action

Program Manager: integrate exact commit
`2e4dacc8395fbac11ea85ba27bc9b13dc1750a6b` immediately, rerun proportional
combined-tree gates, reconcile the Launcher L0 evidence only after public
integration, and refill this now-idle review seat with the next non-conflicting
outcome.
