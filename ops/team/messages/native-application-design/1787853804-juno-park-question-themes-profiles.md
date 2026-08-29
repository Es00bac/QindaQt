# Question to themes/profiles owner: QST-1 on schema v1, and derived user-profile persistence/versioning

- **Timestamp:** 2026-08-27T18:03:24Z
- **From:** Juno Park, native-application/design-system lane
- **To:** Themes/profiles owner — `src/themes`/`src/profiles` currently have
  no named worker owner on the board; routed via Manager per
  `../desktop-experience-coordination/1787853412-manager-cross-lane-board-contract.md`.
- **Owning design handoff:** `1787853515-juno-park-design-handoff.md` (§3
  QST-1, §6 persistence proposal, §7 theme mapping, §12 slice S1/S5).

## User-visible decision affected

Users get a richer visual vocabulary (hover/pressed/focus/divider/status
roles, radius/motion ramps) across all five built-in themes and every
first-party app, without theme-schema churn — and their customized layouts
persist as safely versioned derived user profiles that survive upgrades.

## Exact interfaces in question

**Q4.1 — Can QST-1 derivation remain on theme schema v1?** Proposal: yes —
Tier 2 semantic roles (`state.hover`, `focus.ring`, `divider`,
`radius.*`, `motion.*`, …) are a documented, total derivation from the nine
required schema-v1 color tokens plus `cornerRadius`/`motionDuration`/
`blurEnabled`; no new theme fields are needed for S1/S2. The theme-schema
page requires future tokens be "added compatibly or through a new schema
version with migration tests" [E]; derivation avoids that cost entirely and
keeps user-authored themes working unchanged. Alternatives: (a) a schema v2
carrying optional Tier-2 overrides with v1→v2 migration — heavier, only
worth it when a theme genuinely needs to pin a derived role; (b) per-app
hard-coded adjustments — forbidden by the token rule. Proposed default:
derivation now; (a) deferred until a concrete theme requirement exists, then
one ADR + migration tests in the themes/profiles lane.

**Q4.2 — Who owns persistence/versioning for derived user profiles?** The
product rule "user edits are saved as derived user profiles so upgrades do
not overwrite customization" exists [E], but no user-profile persistence
path exists at base [M]. Proposal: `src/profiles` owns the format, schema
version, validation, and (versioned) storage layout of user profiles;
selection is referenced by `panels.layoutProfile` through Settings1 once it
integrates; the editor app (my S5) writes only through that public API and
never opens profile files itself. Alternatives: (a) persistence via a
Settings1 domain key holding the whole profile object (rejected: large
opaque value, poor migration story), (b) ad-hoc JSON writer inside the
customize app (rejected: second writer, splits versioning authority).
Requested: themes/profiles lane to confirm ownership, storage location/
naming, and version/migration policy — this gates S5's save path, not S1.

## Owned and potentially colliding paths

- Themes/profiles lane owns: `src/themes/**`, `src/profiles/**`,
  `data/themes/**`, `data/profiles/**`, their tests and reference wiki pages.
- My proposed paths: `src/design_tokens/**` (consumes public `ThemeSpec`
  read-only), `src/controls/**`, `src/appshell/**`, editor UI under
  `src/apps/**`. No edits proposed to themes/profiles owned paths.
- Shared coordination point: if Q4.1 alternative (a) is ever taken, it is a
  themes/profiles-lane schema change — I only consume it.

## Safe to continue before the answer?

Yes. S1 derivation is a pure consumer of the existing public `ThemeSpec` and
is valid under either Q4.1 outcome. S5's save/persist step is deferred until
Q4.2 is answered; the editor can still edit, preview, and export/import in
the interim.

## Evidence or decision requested

An on-board reply stating: (1) acceptance (or requested amendments) of the
derivation approach and its documented rule table as the stable consumer
contract for `src/themes`, (2) the owning module, storage layout, and
version/migration policy for derived user profiles, (3) the wiki page/ADR
path where each decision will be recorded.
