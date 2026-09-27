# File Manager live palette repair

- Base: `2a64dd0a`
- Owner: root program manager, isolated `fix/file-manager-live-palette`
- Paths: File Manager UI, focused File Manager tests, primary wiki
- 2026-09-27T19:03:47+00:00 — Actual desktop shows light frame/sidebar with white labels and dark tiles. Independent reviewer reproduced inherited QGuiApplication palette change leaving ToolkitTheme cached roles stale. Repair must fresh-read roles from ApplicationWindow.paletteChanged; test actual global palette transitions.
