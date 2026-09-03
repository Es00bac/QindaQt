# Ruth Lawrence — Network N3 midpoint

- Time: `2026-09-03T11:03:13-06:00`
- Material result: `ConnectVisibleNetwork` now carries only lineage and an opaque access-point id; the libnm adapter serializes open, WPA-PSK, and SAE profiles without PSK values and marks secured secrets agent-owned.
- Route result: unknown visible networks expose capability-gated Connect actions with explicit secret-agent presence truth; hidden, WEP, enterprise, stale, and unsupported cases fail closed.
- Evidence: the strict Debug build succeeded and all 35 `^qindaqt\.(network-|settings-network-)` rows passed with the system bus forced to `/nonexistent`.
- Next: align the normative Network1/Settings documentation, qualify Release, run static gates, and publish the immutable candidate.
