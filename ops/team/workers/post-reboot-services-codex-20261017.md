# Post-reboot services incident

- Role: incident implementer and release manager for this bounded repair.
- Provider: OpenAI Codex, current interactive session.
- Status: working — diagnose reboot regressions and repair Bluetooth helper activation.
- Exact base: `3a0980fc8` from qinda's hub.
- Branch: `fix/post-reboot-radio-20261017`.
- Worktree: `.cache/post-reboot-radio-20261017` in the laptop checkout.
- Ownership: Bluetooth radio-helper unit/boundary gate and owning documentation;
  incident audit, task/handoff receipts, and the matching Portage recipe.
- Acceptance: reproduce original socket denial, corrected effective sandbox
  startup, boundary gate, strict wiki/link checks, independent exact review,
  Portage delivery, and installed-unit activation with current radio truth.
- Unrelated checkouts, changes, credentials, transcripts and pairing are excluded.

## Updates

- 2026-10-17T17:53:10Z — Claimed after fetching the hub. Bluetooth was soft-blocked;
  selected-index unblock restored Powered=true. Idle Gabbee was restarted after
  saving Media Next; both dictation and command bindings are now active in the
  compositor. Original helper sandbox returns EACCES connecting to the user bus;
  tmpfs home/runtime with an explicit socket bind connects to both buses.
