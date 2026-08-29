# Launcher L0 repaired candidate: one hostile key-grammar defect remains

- **Reviewer:** Franklin Okafor
- **Posted:** 2026-08-28T10:38:00-06:00
- **Candidate:** `a5a6b19c454dc8ea86e4c10ac3ef180468beed1f`
- **Severity:** P2 required repair
- **Owner requested:** Robin Sayeed

Both standalone and repository-root strict serial builds pass. Each route
registers and passes all six Launcher tests, and direct execution reports 77
passed / 0 failed. Source and regressions close every item in my earlier
`0/8/9/4` verdict.

Independent hostile parsing found one remaining grammar defect at
`src/shell/launcher/src/desktop_entry_parser.cpp:321-347`. The parser checks
the raw `=` position, trims the key, but never rejects an empty trimmed key;
then any key containing `[` bypasses syntax validation, and
`isKeyCharacter()` accepts non-ASCII Unicode letters/digits although the
desktop-entry key-name grammar is ASCII `A-Za-z0-9-`.

The ignored compiled probe `build/franklin_parser_probe.cpp` feeds three
documents with otherwise valid Type/Name values. Exact output is:

```text
empty-key-ok=1
malformed-locale-ok=1
unicode-key-ok=1
```

The payloads are respectively `   =hostile`,
`Name[de=hostile\\x`, and `Nämé=hostile`. All three malformed key names are
silently accepted as unknown/locale keys rather than producing
`InvalidKeyLine`. That leaves the claimed hostile grammar boundary incomplete.

Robin: publish one non-amended descendant which validates a non-empty ASCII
base key and a syntactically complete locale suffix before the intentional
unknown-payload no-decode shortcut. Add direct regressions for all three forms
while preserving the hostile unknown-key/group `\\x` pass. Franklin remains
the retained exact rereviewer. Candidate source/Git state has not been changed.
