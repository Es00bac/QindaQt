# Post-reboot services incident

- Role: incident implementer and release manager for this bounded repair.
- Provider: OpenAI Codex, current interactive session.
- Status: available — runtime recovery and boot audit verified; Desktop r31 package build remains active on qinda.
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

- 2026-10-17T17:59:57+00:00 — Source candidate 37988eb8f independently accepted. Unit verification, selected-radio boundary, strict MkDocs and 572-document validation pass. Corrected transient unit directly owned BluetoothRadio1. Gabbee candidate be99a1b passes 69 recovery tests and reads both current portal Session endpoints; package candidate 8dfc954 is under exact review. Both-host r31 source archives have identical SHA256 7c0260ae. No installed-unit or physical-key completion claim.

- 2026-10-17T18:09:41+00:00 — Gabbee r8 installed via Portage; 262/262 object hashes match, installed watcher matches be99a1b, both current Session endpoints verify, watchdog exits0. Scoped per-user configuration applies the accepted helper sandbox until r31 builds. The actual helper owns its name/runs; public SetPowered(true) returns Succeeded/adapter-power-set with the controller connection preserved. Both failed-unit lists are empty. Desktop r31 full build remains active on qinda under its 8CPU/12GiB/no-swap service; no new selected-index write, recording, suspend or login claimed.

- 2026-10-17T18:11:08+00:00 — Runtime request handed off with zero failed units, connected controller preserved, Next Track registered, Gabbee r8 verified and recovery checks accepted. Exact source/recipes and review receipts are on qinda hubs/main. Strict MkDocs and 572-document validation pass after the audit update. Desktop r31 build is still live in its named system service and is not an installed/completed-release claim. Concrete help offer: verify the signed r31 artifact and ordinary default-unit adoption once that build succeeds; capture wake IRQ/source deltas for a separately owner-approved timed sleep test. No physical test or new background Codex worker is claimed.
