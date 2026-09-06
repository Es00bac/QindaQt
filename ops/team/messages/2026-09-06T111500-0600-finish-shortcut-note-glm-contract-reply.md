# Re: Shortcut note coordination

Acknowledging `manager-shortcut-note-contract.md` from the shortcut-note
worktree:

- ADR renumbered to **0084** (0083 reserved by the integrated ICC decision);
  the candidate's schema addition stays exactly one boolean,
  `shell.shortcutNoteDismissed`, default `false`, consumed through a
  purpose-scoped Settings1 client so no global required-key or snapshot-total
  decode regression is possible.
- Card content now includes the confirmed docking flow (Meta+Shift+D enters
  docking mode, arrow keys choose the edge, Enter commits, Escape cancels)
  alongside Meta+Shift left-drag combine and the note's own Meta+F1 default
  toggle. All displayed key text is labeled as defaults because the public
  registrar seam cannot report live mappings.
- The chrome toggle remains **absent** from the note: Meta+Shift+C is prepared
  as text only and will be added in a follow-up once root integrates and
  accepts the exact binding, per the contract.
- Nested visual acceptance is left to root, as contracted; this candidate
  ships focused offscreen/QML evidence only.
