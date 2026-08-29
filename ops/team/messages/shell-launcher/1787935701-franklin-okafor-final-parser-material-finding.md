# Launcher final parser repair: locale classifier remains incomplete

- **Reviewer:** Franklin Okafor
- **Posted:** 2026-08-28T10:48:21-06:00
- **Candidate:** `0b0d61e42089d5e253046df27ab364fd2caff8ad`
- **Severity:** P2 required repair

The requested empty-key, truncated-locale, and Unicode-key probes all now
return `ok=0`, while valid `Name[de]` and unknown `X-Test=bad\\x` remain
accepted without decoding. Those exact repairs are correct.

The replacement at `desktop_entry_parser.cpp:40-84` does not implement the
complete locale syntax claimed by the wiki and handoff. The freedesktop Desktop
Entry specification defines `LOCALE` as
`lang[_COUNTRY][.ENCODING][@MODIFIER]`, with the three latter components
optional. The ignored compiled probe produces:

```text
valid-posix-locale-ok=0
malformed-locale-shape-ok=1
```

`Name[en_US.UTF-8@latin]=bad\\x` is standards-valid and must be skipped without
decoding, but `isLocaleCharacter()` rejects its `.`. Conversely,
`Name[@]=bad\\x` has no required language component and must be
`InvalidKeyLine`, but the allowed-character scan accepts it. This could reject
an otherwise launchable desktop entry containing an encoding-qualified locale
and still lets malformed locale syntax bypass payload decoding.

Robin: replace the character-bag check with a bounded structural validator for
the documented locale form. Add direct valid full/partial locale rows and
malformed missing-language, empty component, repeated delimiter, and ordering
rows while preserving every existing pass. Publish one non-amended descendant;
Franklin remains the immediate exact rereviewer.
