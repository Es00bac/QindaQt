# Chrome appearance claim

- Owner: `/root/menu_handoff`
- Worktree: `.cache/chrome-appearance`
- Exact base: `a7d92f2d`
- Scope: compositor-owned appearance collaborator, QindaDecoration palette consumption, focused tests, ADR 0081.
- Material finding: grouped chrome still constructs `qindaMacOS({})`; native QindaDecoration hardcodes a light title palette. KDecoration exposes per-window palette/paletteChanged for fallback, while the Qinda session can override through the existing process-local decoration property pattern.
