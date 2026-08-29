# Claim: Settings Center navigation S1 (QQ-006.04)

- Implementer: Sylvie Hart (Google Antigravity Vertex ADC, `gemini-3.7-flash-high`, reasoning: high)
- Date: 2026-08-28T14:03:20-06:00
- Branch: `worker/settings-center-navigation-s1-sylvie`
- Exact base: `0760e08e1118d6a8b8101f6d17d271d1b766cc96`
- Worktree: `/mnt/d/QindaQt/worktrees/settings-center-navigation-s1-sylvie`
- Outcome: Transform QindaQt Settings (`qindaqt-settings`) into a genuine modular navigation shell hosting both Notifications and Appearance routes (QQ-006.04).

## Scope & Bounded Plan

1. **Typed Route Descriptor & Registry**:
   - Define bounded route descriptors with stable IDs (`notifications`, `appearance`), localized display names, icons, entry points/components, and category metadata.
   - Deterministic route selection and validation, with strict rejection of hostile/duplicate route IDs and unknown startup intent arguments.
2. **Modular Responsive Navigation Shell**:
   - Two-column wide navigation layout and compact responsive single-column layout using only `QindaQt.Controls` and QST tokens.
   - Seamless direct navigation between Notifications and Appearance without duplicating their domain models or settings client state.
   - Fail-closed handling for unavailable/unregistered route intents.
3. **Keyboard & Accessibility**:
   - Arrow keys, Tab, Enter/Return, Escape traversal with explicit focus management and restoration.
   - Truthful accessible role/name/current-page state.
4. **Testing & Verification**:
   - Add registered C++, offscreen QML, CLI intent parsing, package/RPATH, and poisoned negative control test suites under `tests/apps/settings_center/**`.
   - Update primary wiki documentation (`docs/wiki/apps/settings-center.md`) and normative docs if needed.
   - Full strict Debug and Release builds under `/mnt/d/QindaQt/builds`, doc validation, source-shape verification, and clean provenance.
