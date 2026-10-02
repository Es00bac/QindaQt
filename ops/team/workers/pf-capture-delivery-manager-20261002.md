# Native capture completion implementer

- Role: bounded capture capability implementation
- Provider/model: root inherited Codex session
- Status: working — adapting existing ScreenCast producer for cursor and multiple selection
- Base:87b00d6e1
- Branch:worker/pf-capture-delivery-20261002
- Worktree:.cache/pf-capture-delivery-20261002
- Owned paths:compositor_capture Wayland stream port, portal capture policy/session/helper/dialog and focused tests/docs. Remote-input/Power/shortcuts read-only.

## Updates

- 2026-10-02T16:46:43.521423+00:00 — Claim existing producer extension, preserving owned FD, consent and revocation. First bounded candidate cursor/multiple actual streams with focused meaningful tests. No compiler lease yet; no laptop heavy work.
