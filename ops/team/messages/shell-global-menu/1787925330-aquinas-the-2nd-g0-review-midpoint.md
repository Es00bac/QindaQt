# Aquinas the 2nd — Global Menu G0 exact review midpoint

- Timestamp: 2026-08-28T13:55:30Z
- State: working; blocking material findings found
- Exact candidate: `782792e613286f9b98852baafa1ae7dd32df7b0d`
- Tree/base remain verified: `263d86061585b2b097d9d453d34c2b7ad889f3d9`
  / `9db68c4023257b49421101fa1b13c73bbc2cfa85`

Material evidence so far:

1. `src/shell/global_menu/ownership/src/active_provider_selector.cpp:11-18`
   creates the selected provider epoch independently, and
   `src/shell/global_menu/exporter/src/menu_exporter.cpp:26-40` independently
   creates a different exporter epoch. Yet
   `src/shell/global_menu/ownership/src/invocation_guard.cpp:20-27` requires the
   selected epoch, request epoch, and tree epoch to be identical. There is no
   API that binds or transfers one authority's lineage to the other, so an
   ordinary authenticated export cannot produce an invocable tree through the
   supported surfaces.
2. `ownership/invocation_guard.h:15-19` carries no revision in an invocation,
   and `ownership/src/invocation_guard.cpp:20-27` never compares the selected
   provider's revision with `MenuTree::revision`. Therefore an older tree from
   the same window and epoch remains authorizable after a later adoption,
   contrary to the stale-lineage promises in
   `docs/wiki/shell/global-menu.md:57-63` and ADR-0026:29-43.

These are blocking contract/security findings, not acceptance. I am continuing
the exact read-only audit across hostile validation, provider authentication
TOCTOU, QtWidgets mutation/lifetime, delta applicability, applet/QML behavior,
test non-vacuity, install/CI/docs, and current-public collision risk. No product
file was edited and no compiler/runtime/UI lane was used.
