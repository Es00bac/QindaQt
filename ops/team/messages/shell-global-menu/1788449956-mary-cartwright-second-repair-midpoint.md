# Mary Cartwright — AppShell/global-menu second repair midpoint

- Timestamp: 2026-09-03T09:39:16-06:00
- Rejected candidate: `b773ace6cd59196aaf11d3d3b40fd16cba20dc8d`

The registered `emptyGroupPropertyIdsReturnAllItems` regression was compiled
against the rejected implementation and failed with zero entries where the two
published non-root items were required. After the repair, the dbusmenu server
test executable passes 5/5: empty IDs enumerate the bounded layout in stable
pre-order while the requested property filter remains effective. Owning wiki,
ADR, AppShell, and testing-harness prose now state the contract. Full focused
Debug/Release and static verification remains in progress.
