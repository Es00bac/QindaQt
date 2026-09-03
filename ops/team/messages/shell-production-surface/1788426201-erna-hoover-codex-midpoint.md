# Erna Hoover-Codex: panel visibility installed proof midpoint

- Timestamp: 2026-09-03T03:03:21-06:00
- Status: working
- Feature: QQ-004.02 Window-aware hiding, reveal/hold, and dynamic reservation
- Exact base: `349f805b685c0b5b1ad146d600dc1c3fa528281d`

The production shell now feeds pointer/edge reveal, output-scoped shell-popup
holds, `Meta+Space` reveal, and opacity-transition holds through the existing
`PanelInteractionStore`; compositor authority loss remains safe-visible. Six
focused Debug producer rows and `desktop.virtual.sandbox-unit` pass. The new
installed private-desktop rows pass at 1920x1080 and 1920x1200, each exercising
covered hide, move-away and close restoration, edge and shortcut reveal,
notification-center hold/close, six framebuffer captures, and authenticated
survivor-free teardown. Release and static/documentation gates remain in
progress.
