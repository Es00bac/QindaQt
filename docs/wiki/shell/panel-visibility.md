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

That same accepted `(owner, epoch, revision)` lineage fences the separately
authenticated production window-action client, but does not turn this pure
visibility module into a mutation owner. See
[CompositorShell1 actions](../reference/compositor-control-v1.md#authenticated-production-shell-actions)
and [ADR-0061](../adr/0061-authenticate-shell-window-actions-by-panel-owner.md).

For an accepted generation, `shell_orchestration` requires an exact bijection
between compositor outputs, the solved layout, Qt screens, profile panel
expansion, and interaction identities. It then evaluates once and publishes
mapping plus reservation as one controller revision. Eligible visibility-only
changes are applied to existing layer windows in place, preserving QML and
applet state; a static role/layout change still uses complete-set replacement.

Reveal and visibility-hold state uses independent move-only leases so one menu,
pointer region, shortcut, or animation cannot clear another producer's intent.
The production shell now owns the following producers behind that store:

- panel-window containment and one-pixel layer-shell edge sensors acquire a
  reveal lease; pointer departure releases it after the bounded
  `panels.autoHideDelayMs` setting;
- shell popup windows, including the notification center, and applet `Popup`
  objects, including launcher and power popups, acquire output-scoped
  visibility-hold leases for their complete visible lifetime;
- the stable `qindaqt_reveal_panels` KGlobalAccel action requests a reveal for
  every hideable panel with default `Meta+Space`; and
- an opacity transition holds a panel mapped until a hide animation completes,
  then requests the authoritative plan again before unmapping. Reveal animates
  the already-authorized mapped surface back to full opacity.

The runtime reads only exact typed settings. Missing or malformed settings keep
the safe defaults: reduced motion is enabled and the leave delay is 250 ms.
Reduced motion caps the selected theme duration at 80 ms; normal motion uses the
theme duration, bounded to one second. Loss or rejection of compositor
authority cancels transitions, restores full opacity, and leaves policy in its
existing safe-visible state. Producers never set mapping, reservation, or
window inventory directly.

## Installed interaction qualification

The private contained desktop harness qualifies the installed production shell
and compositor in two serial rows:

| Selector | Output | Required interaction evidence |
| --- | --- | --- |
| `desktop.virtual.panel-visibility.single-1080p` | 1920x1080 at 100% | all six phases below |
| `desktop.virtual.panel-visibility.single-wuxga` | 1920x1200 at 100% | the same phases on the S3 WUXGA geometry |

Each row maps a painted client and requires a real `intelligent` left panel to
hide under fullscreen overlap, restore after a private-seat Meta-drag moves the
client clear, and remain restored after the client closes. It then requires the
real `always` bottom panel to reveal through its private edge sensor and through
the exact `Meta+Space` action. Opening the production notification center with
private-seat `Meta+N` must keep that panel visible beyond the shortcut lease;
closing the center must release the popup hold and hide it again. At every
phase, the `never` top panel must remain mapped and committed with exclusive
zone 30, proving that its reservation is retained.

The driver archives six checksum-validated, nonuniform private-parent
framebuffer captures together with exact compositor surface inventories,
authenticated process evidence, bounded cleanup phases, and a final observed
empty survivor set. `desktop.virtual.sandbox-unit` remains a prerequisite and
no host display, bus, input node, uinput device, or hardware is used.

These rows qualify 100% 1080p and WUXGA on the private Weston/KWin path. They do
not claim fractional scaling, multi-output behavior, GPU/OpenGL rendering,
physical input, visual-baseline matching, popup placement aesthetics, or other
profiles and themes; those remain separate matrix and release concerns.
