# ADR-0274: Display1 reads mirroring and modes from the compositor inventory

- **Status:** Accepted
- **Date:** 2026-09-25
- **Owners:** KWin output inventory and the Display1 service
- **Supersedes:** None
- **Superseded by:** None

## Context

Hot-plugging a 1920x1200 monitor into the 1920x1080 `qinda-top` panel mirrored
the two displays, and Settings showed "Display service unavailable", so the
mirror could not be undone and no resolution could be chosen.

KWin had restored a setup it remembered for that pair in which `eDP-1`
replicates `DP-1`. For a mirror KWin reports the **source's** logical geometry
and fits the mirror with a synthetic scale — 0.9 here. `Compositor1.Outputs`
carried neither the replication source nor the real mode, so Display1 saw an
extended output at scale 0.9. That is below Display1's 1.0 minimum, so the
decoder rejected the whole frame and the service withdrew its snapshot. Even
at an equal scale, the mirror would have looked like two overlapping outputs.

The same inventory carried only the current mode. Projection synthesized a
one-entry mode list, so the Resolution control in Settings could never offer a
different resolution. [ADR-0190](0190-mirroring-is-one-field-on-the-mirrored-output.md)
assumed the mode list held every advertised mode. It did not.

The rest of the stack already modelled both. The writer maps
`replicationSourceStableId` and any `current:WxH@mHz` mode id to the KWin
output-device protocol, and topology validation understands mirror graphs.

## Decision

`Compositor1.Outputs` schema 1 gains three additive members per output (wire
1.2):

- `modeSize` — the current mode in untransformed pixels;
- `modes` — the advertised modes (`width`, `height`, `refreshRateMilliHz`,
  `preferred`). The list is deduplicated by size and refresh, bounded to
  Display1's 128 per output, and always includes the current mode;
- `replicationSource` — the **connector name** of the output being mirrored,
  or empty. The KWin plugin resolves KWin's source UUID within the same sample
  and reports "none" when the source is not an enabled, non-mirroring output
  in that sample.

Display1 projects these members directly:

- A mirrored output gets `replicationSourceStableId`, its source's position,
  its real mode from `modeSize`, and its scale clamped into 1.0–3.0. Only a
  mirrored output may arrive with a scale below 1.0; an extended output keeps
  the strict range.
- The output's `modes` become the Display1 mode list. The current mode is
  kept even when truncation drops entries.
- An extended output uses `modeSize` only when it reproduces the reported
  geometry at the reported scale. Otherwise it keeps the geometry-derived
  mode. This means compositor rounding at a fractional scale can never
  withdraw the inventory.
- Absent members mean an older compositor, and the previous projection is
  used.

Mode ids stay in the `current:WxH@mHz` form that the writer already parses,
for every advertised mode.

## Consequences

- If KWin mirrors on hot-plug, Display1 stays available and Settings can
  choose Extend. The resolution control lists real modes.
- The KWin plugin and the Display1 service must ship together for either
  behaviour. A new service works with an old plugin and stays in the
  single-mode projection, but an old plugin cannot stop a KWin mirror from
  withdrawing Display1.
- Display1 still withdraws its snapshot when a frame fails validation, for
  example when an extended output has scale below 1.0. That fail-closed rule
  is unchanged here.
- Evidence: `compositor.kwin-output-inventory` covers wire publication and
  rejection. `qindaqt.display-service-inventory-mirroring` covers the live
  frame captured on `qinda-top`, projection, service availability, and an
  Extend candidate staged through the model. The nested KWin mirror row in
  the [testing harness](../development/testing-harness.md) remains later work.

## Revisit when

- KWin allows mirror chains, or it mirrors onto an output that is not a
  desktop output.
- Display1 needs to distinguish two timings that share a size and refresh
  (for example, reduced blanking).
- Display1 moves off connector-fallback identity, which would let
  `replicationSource` carry a stable ID instead of a connector name.
