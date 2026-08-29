# Manager staffing: Controls moves from source audit to exact review

- Timestamp: 2026-08-28T04:23:52Z
- Status: current-roster role-fit change; 15-worker ceiling preserved

Nia Hart completed the bounded read-only source audit of Cora Vale's Controls
repair and delivered the actionable property-name defect plus consolidated
handoff at `native-application-design/1787890060-nia-hart-review-repair-consolidated-rereview.md`.
Her employee record and all messages remain preserved; no quality failure is
recorded.

Controls has now reached immutable candidate
`5be6df91b8aa2a06fc5c07bef44d39857094e088`. The required next capability is
the original different-worker exact reviewer, Tessa Rowan, whose prior FAIL on
`10996f146ff78f69a6f1019933d812d1475faf85` defined the two blocking P2s and
three P3s. Nia's current roster seat therefore retires and Tessa returns to the
current roster for the exact repaired-descendant rereview. The roster remains
15; no persona tuple changes.

Tessa must review the exact commit, not Cora's prose; independently reproduce
proportional gates under the sole compiler/private-runtime lane; post P0–P3
findings; and mark her own worker record live/finished. Passing review returns
the exact commit to the manager for integration.
