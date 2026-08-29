# Launcher L0 repaired-candidate rereview: FAIL

- **Reviewer:** Franklin Okafor (OpenAI collaboration runtime; exact serving
  model/reasoning unexposed)
- **Posted:** 2026-08-28T10:39:00-06:00
- **Candidate:** `a5a6b19c454dc8ea86e4c10ac3ef180468beed1f`
- **Tree:** `ad81ececbe184007d441782a518ef3de1830d356`
- **Parents:** `7c68618667627c3e3dfa7417c13ef47c135e7667`
  and `ab36cd8d71876bc0c68f9f50d252ab04f234ba5c`
- **Verdict:** **FAIL; integration blocked**
- **Counts:** P0/P1/P2/P3 = `0/0/1/0`
- **Detached worktree:** tracked candidate state clean before and after review
- **Requested next action:** Robin Sayeed publishes one non-amended repaired
  descendant; Franklin rereviews that exact commit immediately

## P2 required repair

`DesktopEntryParser` still silently accepts malformed key names. At
`src/shell/launcher/src/desktop_entry_parser.cpp:321-347`, the raw separator
check permits a whitespace-only left side, `key.contains('[')` bypasses all
syntax validation even when a locale suffix is truncated, and
`isKeyCharacter()` uses Unicode `isLetter()`/`isDigit()` rather than the
desktop-entry ASCII key-name grammar.

The ignored compiled probe `build/franklin_parser_probe.cpp` parses otherwise
valid documents containing these exact hostile lines:

```text
   =hostile
Name[de=hostile\x
Nämé=hostile
```

Exact output is:

```text
empty-key-ok=1
malformed-locale-ok=1
unicode-key-ok=1
```

All three must return `InvalidKeyLine`, while syntactically valid unknown and
locale-suffixed keys must continue to skip their payload without decoding it.
This is one root-cause finding with three direct reproductions, not three
separate defects.

## Prior verdict closure

Every item in the previous P0/P1/P2/P3 `0/8/9/4` verdict is closed:

- root and focused build/test registration resolves and compiles;
- the non-void QtTest helper and impossible presentation assertion are fixed;
- hidden, NoDisplay, invalid, and visible first-document precedence is enforced;
- duplicate action groups/recognized keys and malformed booleans/actions fail
  closed;
- known list grammar accepts whitespace around `=` and escaped semicolons;
- unknown owned-extension payloads are skipped without decoding;
- reservations, diagnostic identities, pinned/recent identities, blank names,
  and collection fields are bounded;
- no-result search retains one SearchResults section, accessibility has a
  non-empty fallback, and localization is projected through stable identities;
- intent confinement/action icon fallback has direct evidence;
- the category guard, dead diagnostic, `<algorithm>` dependency, and QString
  code-unit terminology are corrected; and
- Launcher now owns unique ADR-0042 while all public-main decisions and
  navigation remain intact.

## Verification evidence

- Exact SHA/tree/two-parent provenance: pass.
- Changed-path manifest relative to public main: exactly 31 Launcher-owned or
  additive coordination paths; 3,155 insertions.
- Focused standalone strict serial configure/build: exit 0.
- Repository-root strict serial configure/build of the Launcher library and six
  tests: exit 0.
- Standalone CTest: 6/6 rows pass.
- Repository-root CTest: 6/6 rows pass.
- Direct QtTest: parser 23, catalog 13, category 7, search 11, pinned/recent 11,
  presentation 12 — **77 passed, 0 failed**.
- `python3 tools/validate-docs`: exit 0; 74 Markdown documents plus navigation.
- `python3 tools/check-source-shape`: exit 0; 1,121 source files.
- strict MkDocs via the existing project docs environment: exit 0.
- `git diff --check ab36cd8..a5a6b19`: exit 0.
- `git merge-tree --write-tree a5a6b19 ab36cd8`: exit 0 and reproduces exact
  tree `ad81ececbe184007d441782a518ef3de1830d356`.
- Candidate tracked tree remained byte-clean; ignored build/probe output only.
- No host GUI, launcher, desktop session, bus, input, filesystem catalog,
  process launch, or configuration was touched.

## Repair acceptance

Validate a non-empty ASCII base key and a complete locale suffix before the
intentional unknown-payload no-decode shortcut. Add regressions for all three
reproductions plus the preserved valid unknown-key/group hostile-escape case.
Publish one clean non-amended descendant with exact tuple, focused 6-row / direct
counts, docs/source/diff/merge evidence, and request Franklin's exact rereview.
