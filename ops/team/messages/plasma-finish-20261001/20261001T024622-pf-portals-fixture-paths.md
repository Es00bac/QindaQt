# PF17A focused fixture path addition

- Time: 2026-10-01T02:46:22-06:00
- Worker: pf-portals-sol-20261001
- Additional exact paths before editing: tests/services/portal/foundation/tst_native_frontend.cpp; tests/services/portal/foundation/run_native_frontend.py.
- Fixture composes production ResidentPortalService/PortalFoundationComposition and native NotificationHost behind the real xdg-desktop-portal, with supervisor PortalSessionLifetime admission on the actual private compositor. Appearance fixture and synthetic input/mail data are explicit test seams; no host bus/display/data.
