# Panel visibility policy

QindaQt evaluates automatic panel and dock hiding from one immutable logical
desktop snapshot. The `shell_visibility` module is a pure policy boundary: it
does not own timers, pointer sensing, KWin objects, QML windows, or layer-shell
publication.

## Snapshot contract

One evaluation contains:

- logical output identifiers and rectangles;
- each expanded `(panelId, outputId)` surface rectangle, hide mode, and
  reservation policy;
- application-window frame rectangles, compositor-assigned output, every
  virtual desktop, activities, active, maximized, minimized, and hidden state;
- the current workspace and activity; and
- optional per-surface reveal and visibility-hold requests.

The caller must copy all of those values from one coherent compositor/shell
generation. Identifiers, geometry, references, window scope, active-window
cardinality, and enum values are validated before any result is emitted. One
bad member rejects the complete batch; partial visibility publication is not a
supported state.

## Hide modes

Only non-minimized, non-hidden application windows on the current workspace
and activity participate. Windows may belong to multiple virtual desktops. An
all-workspaces window and a window with no activity list participate in every
matching evaluation.

| Mode | Hidden when |
| --- | --- |
| `never` | Never |
| `always` | No reveal or visibility hold is active |
| `dodge-active` | The active relevant window intersects the actual panel surface |
| `dodge-all` | Any relevant window intersects the actual panel surface |
| `maximized` | A relevant fully maximized window is assigned to the panel's output |
| `intelligent` | The active relevant window intersects the panel, or a relevant fully maximized window is assigned to its output |

Rectangle intersection uses desktop-logical coordinates and the actual panel
surface, never the reduced work area. This matters for partial-width panels,
negative-coordinate outputs, and windows spanning output boundaries. A
spanning window may dodge a panel on an output other than its
compositor-assigned output; maximized policy deliberately uses the assigned
output instead.

`never` remains visible regardless of transient requests. For every other
mode, a visibility hold has priority over an edge/shortcut reveal, and either
request forces the surface visible. Inventory order deterministically selects
the diagnostic trigger when several windows qualify.

## Reservation result

Each decision includes both visibility and a typed reservation intent. A
visible `reserve-when-visible` panel requests `Reserve`; hidden panels and
`never-reserve` panels request `Release`. This keeps the policy explicit while
leaving atomic mapping, animation, and layer-shell exclusive-zone changes to a
shell controller.

## Production integration

The KWin plugin publishes one bounded, revisioned output/window/scope snapshot
through `Compositor1`. The shell client subscribes to the exact D-Bus unique
owner before exposing it, coalesces invalidations, permits one request in
flight, rejects stale owner/epoch/revision replies, and uses bounded timeout and
retry delays. Service loss, malformed/unavailable data, revision regressions or
collisions, and output-generation races immediately select the safe-visible
policy. Forward revision gaps are valid because invalidations are coalesced and
every payload is a complete snapshot.

For an accepted generation, `shell_orchestration` requires an exact bijection
between compositor outputs, the solved layout, Qt screens, profile panel
expansion, and interaction identities. It then evaluates once and publishes
mapping plus reservation as one controller revision. Eligible visibility-only
changes are applied to existing layer windows in place, preserving QML and
applet state; a static role/layout change still uses complete-set replacement.

Reveal and visibility-hold state uses independent move-only leases so one menu,
pointer region, shortcut, or animation cannot clear another producer's intent.
The store and runtime path are active, but production edge sensors, pointer
containment, menu leases, hide animation, and shortcut producers remain to be
wired. Consequently the compositor-driven `dodge-*`, `maximized`, and
`intelligent` decisions are implemented, while the user-reveal path required
for a usable `always` panel is not yet qualified.
