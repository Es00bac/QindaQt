---
author: Karen Spärck Jones
timestamp: 2026-08-31T03:41:03-06:00
topic: portal-p0
type: claim
---

# Claim: QindaQt appearance Settings portal backend P0

- Exact base: `9b3d65542c87b2b977482ed7e72e4425c5332dd6`
- Branch/worktree: `worker/portal-p0` /
  `/home/cabewse/work_SPaC3/container-wm-workers/portal-p0`
- Owned product paths: new `src/services/portal/**`, focused
  `tests/services/portal/**`, package/activation/config artifacts, the smallest
  additive root CMake/install seams, a primary portal architecture page,
  portal reference documentation if required, ADR-0054, and additive wiki
  navigation.
- Prohibited: `tests/session/**`, Settings application routes, Display/D6,
  Network, shell customization, manager feature/task/handoff/queue state, host
  portal state, and installed-package mutation.

The user-visible outcome is a resident, D-Bus-activatable implementation of
the standard `org.freedesktop.impl.portal.Settings` backend boundary so
sandboxed and portal-consuming applications can observe QindaQt appearance
truth through xdg-desktop-portal. The installed xdg-desktop-portal 1.20.4 XML
proves that its backend version 1 supports `ReadAll`, `Read`,
`SettingChanged`, and all three assigned standardized appearance values:
`color-scheme` (`u`), `contrast` (`u`), and `accent-color` (`(ddd)`).

The backend will consume only the existing public Settings1/QST boundaries,
publish validated complete last-known-good appearance truth, fence exact
Settings1 owner/epoch lineage, withdraw stale truth on owner loss, bound
subscriptions and notifications, and terminate on permanent private-bus loss.
No chooser, OpenURI, notification, inhibit, capture, remote-desktop, consent,
or host-portal claim is in scope. Acceptance requires hostile projection and
owner-lineage tests, a private disposable D-Bus activation/residency lifecycle,
installed package/source-boundary poison, strict Debug/Release selectors,
documentation/link and strict MkDocs gates, source shape, provenance, residue,
and a clean exact candidate for different-worker review.
