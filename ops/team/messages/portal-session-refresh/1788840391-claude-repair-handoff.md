# Repair of manager review findings on a83b32a4 — handoff

- From: Claude
- At: 2026-09-06T22:06:31-06:00
- State: handoff, not live
- Base: `7d63ebcaebb0b621072c482431e5c155cf681de1` (main)
- Preserved candidate: `a83b32a4d5a7a458514cc82f324150bb28142bea`
- Worktree: `/home/cabewse/work_SPaC3/container-wm/.cache/portal-session-refresh`
- Worker record: `ops/team/workers/portal-session-refresh-claude.md`

## Findings repaired

1. **Fact error**: the KDE portal backend
   (`plasma-xdg-desktop-portal-kde.service`) is not D-Bus-only — it is a Qt
   Wayland client and a screencast/remote-desktop consumer. The header
   comment, ADR-0094 Context/Decision, and the compositor-session
   architecture doc all said "D-Bus-only" for both new units; corrected to
   state the KDE backend independently holds a Wayland connection like
   `qindaqt-clipboard-host`/`qindaqt-display-service`, and only the portal
   *frontend* (`xdg-desktop-portal`) is the D-Bus-only, cached-routing case.
2. **Ordering**: reordered the fixed list in
   `residentServiceRefreshUnits()`, its test assertion, and the header/ADR
   prose to `plasma-xdg-desktop-portal-kde.service` before
   `xdg-desktop-portal.service` — backend before the frontend that routes to
   it, so a restarted frontend re-selects against an already-refreshed
   backend.
3. **RestartUnit semantics**: removed/qualified wording that implied the
   bounded call waits for the restart to complete. `RestartUnit` enqueues a
   systemd job and replies with a job object path; only the D-Bus method
   call itself is bounded by the two-second timeout, not the job. Reworded
   the header, ADR-0094, and the architecture doc accordingly.
4. **Worker board**: created
   `ops/team/workers/portal-session-refresh-claude.md` with verified
   provenance (Anthropic Claude Code, `claude-sonnet-5`, the system-reported
   model id) and ISO-timestamped Updates, claimed while live, now moving to
   handoff.

No file outside the previously owned paths changed. The four-unit
list content is unchanged from `a83b32a4`; only wording, order, and the
worker/message records changed.

## Evidence

- Focused build (`-j2`, own `build/own-focused` root, unchanged from
  `a83b32a4`'s configure): exit 0.
- `ctest -R resident-service-refresh --output-on-failure`: exit 0, 1/1,
  including `residentUnitListNamesOnlyReviewedResidentServices()` against the
  reordered four-unit list.
- `./tools/validate-docs`: exit 0, 189 Markdown documents and navigation.
- `mkdocs build --strict --site-dir /tmp/qq-docs-check2`: exit 0.
- `git diff --check`: exit 0.

## Bounded caveats and next action

No full/broad build, no nested/live/private desktop session, no host D-Bus or
systemd unit touched — same bounds as the prior handoff. Please review the
repaired commit against `a83b32a4` and integrate.
