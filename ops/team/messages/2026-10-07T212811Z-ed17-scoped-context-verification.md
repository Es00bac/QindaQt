# ED17 source findings and verification
- Time: 2026-10-07T21:28:11+00:00
- QindaTK source: 393c1ce5c075b3a4dfc90b7b0b58ae80ab3fd7bb; QindaOffice source: f1f3492b38e88c04ac2e2aee724c89874fdd67ea.
- Existing grant authority is application-owned Off/Read/Edit, shared by cooperative same-UID clients. CLI scans sockets; there is no per-task principal or lock gate in the current host/server.
- Existing opaque lifetime/revision, real engine/undo, revoke-before-replay and non-evicting receipt semantics are reused. Scoped mode must be additive and cannot translate narrow consent into v1 broad grants.
- Native portals/capture/input and Voice-only window commands retain separate authority.
- python3 tests/design/desktop_agent_context_contract.py: exit 0, 17 tests. Fixtures assume authenticated identities/privacy; no production or actual provider/window qualification.
- python3 tools/validate-docs: exit 0, 523 documents. mkdocs build --strict and git diff --check: exit 0.
- First implementation routing is exact: QindaTK public scoped policy/host/transport hook, desktop public lock/task composition, one Office Calc Read grant, scoped CLI/MCP; providers own File Manager/Text Editor follow-ons.
- Caveats: distinct-peer/process admission, real provider UI/lock/lifecycle, package and installed graphical/keyboard/CLI/MCP journeys remain to implement and qualify.
