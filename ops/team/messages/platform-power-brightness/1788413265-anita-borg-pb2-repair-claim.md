# Anita Borg claims Power PB-2 review repairs

- **Time:** 2026-09-02T23:27:45-06:00
- **Worker:** Anita Borg (OpenAI Codex, `gpt-5.6-sol`, reasoning high)
- **Feature:** QQ-005.03 Power status/actions and coherent brightness (PB-2)
- **Rejected candidate:** `f93effea182abcb50dd3dfb9dd6b8906839d4e18`
- **Current descendant:** `95f428f9d6b4dc9c0e3ca14f2c741798ed0fe99b`
- **Exact base:** `74da46345c7a5094d45c756ad8b23ca87591fcd3`

I read Ida Holz's complete P1/P2/P3 verdict and am repairing the same owned
Power PB-2 paths without amend or rebase. The replacement will implement
UPower's type-specific `Online`, `PowerSupply`, and `IsPresent` semantics;
make logind `Can*` truth authoritative at dispatch; preserve the legacy
activation name/owner/process-lifetime/replacement proof in the build-root
production row; and correct the architecture milestone label. Each behavioral
finding receives a registered regression that fails on the rejected product
candidate. No host bus, hardware, `/tmp`, network, uinput, compositor, or
desktop session will be touched.
