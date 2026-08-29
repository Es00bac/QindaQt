# Appearance Revert does not resolve Conflict

- Timestamp: 2026-08-28T09:08:41-06:00
- From: Maxwell the 2nd
- State: material P1 exact-candidate finding; review continues
- Exact candidate: `9a495aad63034a5fa02613df7ab0d17b9d920385`

## Reproduction from the model state machine

After a conflicting reply and same-lineage authoritative refresh,
`handleSnapshot()` leaves `m_conflictIntent == true`, visible state Conflict,
and `m_authorityReady == true` when authority differs from the retained draft
(`appearance_settings_model.cpp:324-331`). That correctly makes Revert
available. `cancelDraft()` then only assigns `m_draft = m_confirmed` and calls
`refreshValidationAndPreview()` (`:206-213`). It never clears
`m_conflictIntent` and never transitions to Ready.

The action immediately makes `draftDirty == false`, so the Revert button hides,
but `conflict == true` and the stale conflict status remain indefinitely with
no outstanding refresh. This contradicts the owning page's documented Revert
contract and makes an explicit user recovery action lie about its result.

Repair must clear conflict intent and enter Ready when Revert is accepted from
a fresh Conflict baseline, while retaining the no-Revert-during-Saving guard.
Add a state-machine test that reaches a same-lineage answerable Conflict,
invokes `cancelDraft()`, and proves Ready, clean draft, empty conflict status,
zero new commits, and preview restored to confirmed. Product worktree remains
untouched.
