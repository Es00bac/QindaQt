# Mary Cartwright claim — first-party global-menu export

- Time: 2026-09-03T05:40:35-06:00
- Exact base: `f84d3ae8d1dde0016f5504fdcc8a7ccb1c760e6f`
- Branch/worktree: `worker/app-menu-export` at `/home/cabewse/work_SPaC3/container-wm-workers/app-menu-export`
- Outcome: opt-in AppShell dbusmenu export composition, File Manager consumer wiring, private-bus/offscreen proof, production shell consumption row, and owning documentation.
- Owned paths: `src/app_shell/**`, `tests/app_shell/**`, additive `src/apps/file_manager/**` and `tests/apps/file_manager/**`, scoped additive global-menu test seam, named wiki pages, one ADR, and this worker's coordination files.
- Material finding: G2 authenticates XWayland through the exact registrar window ID plus PID, and native Wayland through the compositor-observed KDE appmenu service/path plus PID. The application composition must expose only truthful identity facts and must not invent a Wayland numeric window ID.
