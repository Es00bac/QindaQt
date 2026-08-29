# Tessa Rowan — Controls S2 repaired-candidate source midpoint

Timestamp: 2026-08-27T22:28:05-06:00

Exact candidate `5be6df91b8aa2a06fc5c07bef44d39857094e088`, tree
`000e58c658f8d17e896d2b88a7c1266bc8d5831c`, is a clean detached direct
descendant of rejected candidate `10996f146ff78f69a6f1019933d812d1475faf85`.
The 14-path diff is Controls source/build/tests plus its two owning wiki pages;
no production shell/service/display path changed and no broad pass is claimed.

The read-only repair audit currently closes both former P2 implementations and
all three P3s: Qt's queried deploy paths drive one-to-one installation and the
exact staged inventory; the staged consumer performs bare, zero-warning
qmllint plus runtime import outside source/build paths; StateCard's readiness
is private and a zero-interval one-shot reads the final status/title/message
tuple after one event turn; the behavior helper rejects synchronous/stale/
duplicate publication and covers same-status Warning/Error content plus
status-before-content; the source policy is a positive four-module allowlist;
both named Noto host families are asserted; and the isolation comment now
matches ADR-0021's separate process-lifetime and QST-motion boundaries.

No source blocker is open. The documented construction-silence behavior is
supported by the readiness gate, but the committed test attaches its spy only
after scene construction and therefore does not directly observe construction.
I will add no candidate source; instead I will independently probe that exact
behavior during the proportional runtime review and classify any remaining
coverage risk from evidence. Serial Debug/Release Controls/package
qualification is next under my sole compiler/private-runtime lane.
