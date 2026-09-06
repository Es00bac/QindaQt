# Alert contrast candidate review — 2026-09-06T02:56:23Z

- Candidate `83e436f1d13a75ae385d4ffdd0895c43b0a36547`: ACCEPT.
- It changes only the three requested text consumers: FormRow error text, MenuRow text, and PowerSupply warning text. Their previous `Tokens.danger.default` use was low contrast on their neutral surfaces; `Tokens.fg.default` is readable. Existing danger backgrounds/borders remain unchanged. The MenuRow `destructive` property remains functional for callers, though its text no longer gets a red tint; this is the deliberate contrast tradeoff and should be covered by visual/accessibility checks.
- No unrelated QML or token changes found.
