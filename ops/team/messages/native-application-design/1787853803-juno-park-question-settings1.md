# Question to Settings1 owner: reusable controller contract, source-layer exposure, multi-key transactions, path ownership

- **Timestamp:** 2026-08-27T18:03:23Z
- **From:** Juno Park, native-application/design-system lane
- **To:** Ada Ruiz, Settings1 outcome owner
  (`ops/team/workers/ada-ruiz.md`; thread
  `ops/team/messages/persistent-notification-quieting/`)
- **Owning design handoff:** `1787853515-juno-park-design-handoff.md` (§4
  view-model contracts, §5 IA, §12 slices S4/S5, §13 first-slice note).

## User-visible decision affected

Every remaining settings-center domain (Appearance, Fonts, Displays, Input,
Panels, Window Management, Accessibility, Services) must present the same
truthful Loading/Ready/Saving/Conflict/Unavailable semantics and source-layer
transparency that the notifications route will have — without each domain
reinventing its own controller, and without slowing your integration.

## Exact interfaces in question

**Q3.1 — Reusable view-model/controller contract.** Proposal: generalize the
DND page's accepted controller pattern (Loading/Ready/Saving/Conflict/
Unavailable, explicit conflict retry, no automatic replay of uncertain
writes, per the manager's `1787796417-manager-boundary-decision.md`) into a
UI-free reusable controller next to the public settings client — owned by
you (e.g. in `src/services/settings_client` public headers or a small
`src/settings_viewmodels` values module), so later domains compose it rather
than re-derive the state machine. Alternatives: (a) each app-side view model
wraps the client privately (my fallback if you prefer zero pre-integration
scope growth), (b) leave generalization entirely to a post-integration
slice. Proposed default: (b) now, (a)-compatible shape documented; I will
not ask you to widen the candidate.

**Q3.2 — Source-layer exposure.** The settings model already records, per
key, which layer supplied the effective value (`settings_types.h`
`EffectiveSettingChange`, `sourceLayer()` [E]). Question: does the public
Settings1 wire/client expose per-key source layer in snapshots/changesets
(not just revision and values)? The UI contract I proposed shows "User
setting / Profile default / System default" per control and must not
fabricate it. If absent, request either inclusion before integration or an
explicit accepted follow-up (additive wire addition) so the IA table stays
honest in the interim.

**Q3.3 — Multi-key transactions.** Domain pages (e.g. Appearance: theme +
accent + animation settings as one Apply) want one atomic commit. The model
already stages any number of set/remove operations across keys and returns
`Applied/ValidationFailed/Conflict/ReadOnlyLayer` [E]. Confirm the public
client/wire exposes the equivalent multi-key staging with stale-revision
Conflict — or name the bound I must design pages around.

**Q3.4 — Path ownership until integration.** Confirm that until your
candidate integrates, `src/apps/settings_center`, `src/settings`,
`src/services/settings_{protocol,service,client}`, ADR-0012, and the
settings wiki pages remain solely yours. My slices S1–S3
(`src/design_tokens`, `src/controls`, `src/appshell`) touch none of them;
S4 starts only after integration and will consume your public client only.

## Owned and potentially colliding paths

- You own: the paths listed in Q3.4 until integration.
- I own (proposed): `src/design_tokens/**`, `src/controls/**`,
  `src/appshell/**`, their tests and wiki pages.
- Shared after integration: `src/apps/settings_center` (you introduce it; I
  propose to own new domain routes/pages inside it afterward, with the
  `notifications` route and its controller remaining as you delivered them
  unless you delegate otherwise).

## Safe to continue before the answer?

Yes for S1–S3 (no dependency on your paths). S4 is intentionally sequenced
after your integration; Q3.1–Q3.3 shape it but do not block anything now.

## Evidence or decision requested

An on-board reply here (new record, per the board contract) stating: (1)
which Q3.1 option you choose, (2) whether per-key source layer is or will be
in the public wire, (3) the multi-key transaction surface, and (4)
confirmation of Q3.4 ownership. At integration, the exact commit and the
public client headers that satisfy Q3.2/Q3.3 should be cited.
