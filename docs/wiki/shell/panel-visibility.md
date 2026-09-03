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
  visibility-hold leases through a bounded owner/lifetime registry. It admits
  at most 32 sources, 128 aggregate surface leases, and 128 UTF-16 units per
  source identity. Every source is fenced to one `QObject` owner and releases
  on close/hide, owner destruction, topology replacement, or producer teardown.
  One uninterrupted admission expires after 30,000 ms even if the producer
  misses close; duplicate visible notifications do not renew it, and only a
  later close/reopen transition can acquire another bounded hold;
- the stable `qindaqt_reveal_panels` KGlobalAccel action requests a reveal for
  every hideable panel with default `Meta+Space`; and
- an opacity transition holds a panel mapped until a hide animation completes,
  then requests the authoritative plan again before unmapping. Reveal animates
  the already-authorized mapped surface back to full opacity.

The final unmap crosses an asynchronous layer-shell boundary. After the
animation lease is released and the shell hides the window, compositor
authority may briefly retain the old mapped/committed role while its geometry
has already collapsed to zero. That record is teardown in progress, not a
hidden panel state and not evidence of another popup lease. In particular, the
hosted Global Menu participates in the ordinary popup lease only while its
popup is visible; its presence does not keep an unrelated panel window alive.
Consumers that qualify a completed visibility transition must wait for a
settled authority snapshot: every published surface geometry is positive and
inside the framebuffer, and a hidden panel's role is absent. A role that never
reaches that state fails closed at the observation deadline.

The runtime reads only exact typed settings. `panels.autoHideDelayMs` is the
canonical signed 64-bit integer produced by the Settings1 codec, while
`accessibility.reducedMotion` is an exact boolean. Missing or malformed
settings keep the safe defaults: reduced motion is enabled and the leave delay
is 250 ms. Reduced motion caps the selected theme duration at 80 ms; normal
motion uses the theme duration, bounded to one second. A private-bus Settings1
round trip pins that production representation. Loss or rejection of
compositor authority cancels transitions, restores full opacity, and leaves
policy in its existing safe-visible state. Producers never set mapping,
reservation, or window inventory directly.

## Installed interaction qualification

The private contained desktop harness qualifies the installed production shell
and compositor in two serial rows:

| Selector | Output | Required interaction evidence |
| --- | --- | --- |
| `desktop.virtual.panel-visibility.single-1080p` | 1920x1080 at 100% | all eight phases below |
| `desktop.virtual.panel-visibility.single-wuxga` | 1920x1200 at 100% | the same phases on the S3 WUXGA geometry |

Each row maps a fullscreen painted client over the real start-aligned,
partial-height `intelligent` left panel. A setup Meta-drag restores it onto that
panel, and the row re-establishes the hidden state at the final boundary before
a second private-seat Meta-drag.
That causal drag must change the client's compositor-reported frame and leave
it clear before the panel may count as restored. The client becomes fullscreen
again to establish a second hidden state; close must remove
the window from compositor authority and restore the panel in a fresh capture. The
row then requires the real `always` bottom panel to reveal through its private
edge sensor and through the exact `Meta+Space` action. Opening the production
notification center with private-seat `Meta+N` must keep that panel visible
beyond the shortcut lease; closing the center must release the popup hold and
hide it again. At every phase, the `never` top panel remains mapped and
committed with exclusive zone 30, proving that its reservation is retained.

The driver archives eight checksum-validated, nonuniform private-parent
framebuffer captures: overlap-hidden, moved-away, close-hidden,
closed-restored, edge-revealed, shortcut-revealed, popup-held, and popup-closed.
Every capture is joined to the exact compositor-authority surface inventory
taken for that phase. Before capture, the probe polls through asynchronous
layer-surface teardown and admits only a settled inventory; a mapped 0x0 role,
malformed geometry, or geometry outside the framebuffer cannot qualify a
hidden phase. Authority-settlement failures and framebuffer-capture failures
are reported separately so a missing capture dependency cannot be mistaken for
a visibility-policy failure.

The system-KWin qualification intentionally combines `/usr/bin/kwin_wayland`
with the private Weston 15 parent. The sandbox-wide loader path belongs to KWin
and excludes the parent prefix; otherwise KWin can load an ABI-incompatible
private `libkwin`. The capture probe therefore gives only its
`weston-screenshooter` child the already-authenticated loader path used by the
parent Weston process. `desktop.virtual.panel-visibility.capture-loader-unit`
rejects a missing, relative, or partially empty path at the Python command
boundary. The separate `capture-loader-cpp-unit` row pins the C++ application
of that path to the screenshot child's `QProcessEnvironment`; removing either
half fails its owning row. The deterministic `validator-unit` row injects a
fake authority and poll timer to prove that a transitional 0x0 role is ignored
until the role is absent, and that a role which never disappears times out
fail-closed. The validator derives stable left and bottom panel
rectangles only from mapped/committed authority records, samples all pixels in
the corresponding rectangle, and requires each hidden/visible pair to have a
different region digest. Identical unrelated images therefore fail even when
their dimensions, checksums, and whole-frame color counts are valid. Canonical
evidence also carries the final pre-drag surface-authority snapshot,
before/after window frames for the drag, the authoritative pre-close frame,
post-close absence, authenticated process evidence, bounded
cleanup phases, and a final observed empty survivor set.
`desktop.virtual.sandbox-unit` remains a prerequisite and no host display, bus,
input node, uinput device, or hardware is used.

The eight-capture rows use a 100-second sandbox-attempt deadline within their
110-second CTest boundary. This preserves ten seconds for outer archival and
CTest cleanup while allowing the complete WUXGA capture validation to finish;
the generic desktop attempt remains capped at 70 seconds.

At terminal cleanup the harness first sends `SIGTERM` to the authenticated KWin
process alone and gives its signal handler a bounded opportunity to own session
shutdown. Only then does it terminate remaining authenticated process groups.
This ordering prevents the harness from racing KWin's `--exit-with-session`
observer with a simultaneous signal to `qindaqt-session`. Any `Session process
has crashed` diagnostic is a failed row, including after capture completion;
the terminal ledger and final survivor observation remain independently strict.

These rows qualify 100% 1080p and WUXGA on the private Weston/KWin path. They do
not claim fractional scaling, multi-output behavior, GPU/OpenGL rendering,
physical input, visual-baseline matching, popup placement aesthetics, or other
profiles and themes; those remain separate matrix and release concerns.
