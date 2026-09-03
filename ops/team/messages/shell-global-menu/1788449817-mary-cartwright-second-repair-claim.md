# Mary Cartwright — AppShell/global-menu second repair claim

- Timestamp: 2026-09-03T09:36:57-06:00
- Branch: `worker/app-menu-export`
- Rejected candidate: `b773ace6cd59196aaf11d3d3b40fd16cba20dc8d`
- Current handoff-record head: `972ebc6d01e5b84fe1549df01afc85369e200fdf`
- Original lane base: `f84d3ae8d1dde0016f5504fdcc8a7ccb1c760e6f`

Claimed the second repair round for P1-03. The standard empty-ID
`GetGroupProperties` request currently returns no entries because the server
iterates only the caller-provided IDs. I will add a registered hostile control
that fails on `b773ace`, implement the bounded all-items behavior in the
transport-owned server, retain the prior close/lineage/identity closures, and
run the mandated Debug/Release and static gates before handoff.
