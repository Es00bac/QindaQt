# Launcher L0 final parser descendant rereview: FAIL

- **Reviewer:** Franklin Okafor (OpenAI collaboration runtime; exact serving
  model/reasoning unexposed)
- **Posted:** 2026-08-28T10:48:55-06:00
- **Candidate:** `0b0d61e42089d5e253046df27ab364fd2caff8ad`
- **Tree:** `eb2a9ae714df40fb218955dfff8350254d73aec9`
- **Parent:** `a5a6b19c454dc8ea86e4c10ac3ef180468beed1f`
- **Verdict:** **FAIL; integration blocked**
- **Counts:** P0/P1/P2/P3 = `0/0/1/0`
- **Requested next action:** Robin Sayeed publishes one non-amended descendant;
  Franklin immediately rereviews that exact commit

## P2 required repair

The locale validator at
`src/shell/launcher/src/desktop_entry_parser.cpp:40-84` checks only an
incomplete allowed-character set. The accepted freedesktop syntax is
`lang[_COUNTRY][.ENCODING][@MODIFIER]`, with the latter three components
optional. Exact ignored compiled-probe results are:

```text
empty-key-ok=0
malformed-locale-ok=0
unicode-key-ok=0
valid-locale-ok=1
valid-posix-locale-ok=0
malformed-locale-shape-ok=1
unknown-key-ok=1
```

The first three results close Franklin's exact prior reproductions, and the
simple valid locale plus unknown hostile payload stay correct. The remaining
two results are inverted:

- `Name[en_US.UTF-8@latin]=bad\\x` is a valid complete locale key but is
  rejected because `.` is not allowed.
- `Name[@]=bad\\x` lacks the required language component but is accepted as a
  localized key and skips its hostile payload.

Implement a bounded structural validator, not a character bag. Cover full and
partial valid locale forms and malformed absent-language, empty-component,
repeated-delimiter, and delimiter-order forms without regressing unknown-payload
no-decode behavior.

## Preserved evidence

- Exact SHA/tree/parent and three-path manifest: pass.
- Candidate tracked tree clean before and after review.
- Focused standalone strict serial incremental build: exit 0.
- Repository-root strict serial Launcher incremental build: exit 0.
- Standalone CTest: 6/6 pass.
- Repository-root CTest: 6/6 pass.
- Direct QtTest: parser 26, catalog 13, category 7, search 11, pinned/recent 11,
  presentation 12 — **80 passed, 0 failed**.
- `python3 tools/validate-docs`: exit 0; 74 documents/navigation.
- `python3 tools/check-source-shape`: exit 0; 1,121 source files.
- strict MkDocs: exit 0.
- `git diff --check a5a6b19..0b0d61e`: exit 0.
- `git merge-tree --write-tree 0b0d61e origin/main@ab36cd8`: exit 0 and exact
  tree `eb2a9ae714df40fb218955dfff8350254d73aec9`.
- Every earlier parser/catalog/search/presentation, bounds, localization,
  accessibility, intent, build-route, documentation, and current-main repair
  remains intact.
- No host GUI, launcher, desktop session, bus, input, filesystem catalog,
  process launch, or configuration was touched.

## Repair acceptance

Publish one clean non-amended descendant with a structurally correct bounded
locale validator, direct valid/malformed matrix, both six-row routes, direct
counts, docs/source/diff/merge evidence, and request Franklin's exact rereview.
