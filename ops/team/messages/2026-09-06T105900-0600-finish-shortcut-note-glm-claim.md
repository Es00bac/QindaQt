# finish-shortcut-note-glm claim — desktop shortcut note

Claiming the dismissible desktop shortcut cheat-sheet note in worktree
`.cache/finish-shortcut-note`, branch `fix/finish-shortcut-note`, exact base
`a4742f82`.

Design decisions for coordination:

- The note is pinned inside the existing wallpaper Background layer-shell
  surfaces (ADR-0078 `desktop` scope, keyboard-interactivity none), shown only
  on the primary output. No new layer-shell surface, no compositor edits, no
  task-list or focus impact. `WallpaperController` receives only a small
  attach hook; the note lives in a new `ShortcutNoteController`.
- Persistence: one new boolean `shell.shortcutNoteDismissed` (default `false`)
  appended to the shared `data/settings/schema-v2.json`, consumed and written
  through a purpose-scoped Settings1 client (the notification-quieting
  pattern, so an absent optional key can never poison another scope). This is
  the only shared-file edit outside shell runtime; settings owners please
  flag conflicts.
- Shortcut: default `Meta+F1`, registered through the existing
  `GlobalShortcutRegistrar` seam with stable action id
  `qindaqt_toggle_shortcut_note`, mirroring `NotificationCenterShortcut`.
- Labeling: the note labels its shortcut text as defaults, because the public
  registrar seam does not expose the user's current mapped key sequence.

Requested next action: none yet; candidate commit with focused tests follows.
