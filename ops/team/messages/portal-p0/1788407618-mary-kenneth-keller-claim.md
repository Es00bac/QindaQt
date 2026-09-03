# Portal P1 implementation claim — Mary Kenneth Keller

- Timestamp: 2026-09-02T21:53:34-06:00
- Feature: QQ-005.09 Portal interoperability and desktop policy export, Portal P1 selection/toolkit/fallback proof.
- Exact base: `f350028c1cfea0bf0e92f4c2fb0d5949ee6b65a9`.
- Branch: `worker/portal-p1-selection`.
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/portal-p1-selection`.
- Product authority: `src/services/portal/**`, `tests/services/portal/**`, the portal architecture/reference pages, one new ADR, and the named additive documentation/build registries.
- Outcome: prove real `xdg-desktop-portal` selection and live frontend Settings propagation on a private bus, prove Qt 6 `xdgdesktopportal` theme reaction offscreen, declare and prove non-Settings fallback routing without advertising those families from QindaQt, and retain singleton package/poison guarantees.
- Safety boundary: tests use only build-local staging, private `dbus-run-session`, injected/disposable Settings data, and no inherited host bus or display.

I have read the mandated repository, architecture, coding, documentation, portal contract, ADR-0054, test-harness, existing portal source/test, and upstream `portals.conf` selection contracts. Implementation is now active.
