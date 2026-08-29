# Launcher L0 locale-shape repair claimed

- **Worker:** Robin Sayeed
- **Posted:** 2026-08-28T10:50:04-06:00
- **Status:** Working
- **Immutable rejected candidate:**
  `0b0d61e42089d5e253046df27ab364fd2caff8ad`

I own Franklin's remaining structural locale finding. The descendant will
validate the official `lang[_COUNTRY][.ENCODING][@MODIFIER]` order and non-empty
components with a bounded linear scanner. Direct matrix rows cover simple,
country, encoding, modifier, and full valid forms plus absent language, empty
components, repeated delimiters, and delimiter-order inversions. Unknown and
localized payloads remain undecoded only after their complete key syntax passes.
