# Native semantic window-management commands

The semantic command boundary is shared by native input adapters and the voice
provider. The policy and production transport pass focused private-bus/geometry tests and real private native scene qualification at normal and actual 150% scale. Gabbee consumer31147a55 is integrated as eadb8995 with manager88 tests and20 subtests; grouping and application launch remain the next acceptance boundary. `src/window_management` implements bounded
typed decoding, fractional geometry and expiring command admission. The native
transport and compositor executor remain uninstalled. The accepted
choice is [ADR-0301](../adr/0301-share-semantic-window-management-with-voice.md).
The separate [application SDK](application-window-management.md) is executable
and retains its same-connection surface authority.

## Ownership and lifetime

`QindaQt::WindowManagement` is a Qt Core value/policy library. It owns no KWin,
QML, microphone, application-launch or persistence object. `Controller` borrows
explicit `Authority`, `Scene` and `Executor` interfaces; they outlive it and all
calls run synchronously on their common owning thread. The platform adapter owns
window discovery, atomic topology/geometry execution and launch correlation.
Gabbee owns speech recognition and parsing; raw transcripts do not cross this
window-management boundary. Dictation never dispatches window management.

A command-mode hotkey captures the current foreground subject before listening.
Contexts last at most 60 seconds, belong to the exact live provider unique bus
owner, and are single-use. A new capture replaces that provider's old context.
Owner replacement, confirmed Settings1 opt-out, lock and teardown invalidate
contexts. Admission checks authority and lock before decoding or target lookup;
stale foreground/topology ownership fails without retargeting. At most sixteen
contexts/rate records exist, with eight admitted requests per owner per second.
Submitted contexts are consumed even on invalid, ambiguous or unavailable results.

## Versioned wire contract

The production source endpoint is service `org.qindaqt.Compositor`, object
`/org/qindaqt/WindowManagement`, interface `org.qindaqt.WindowManagement1`.
It is distinct from development control and is not enabled by a development flag.

| Method | Inputs | Result |
| --- | --- | --- |
| `BeginCommand` | none | JSON bytes containing version, status, contextId, captured windowId/containerId and currently implemented capabilities |
| `ExecuteCommand` | contextId string, request JSON bytes | versioned status, message and optional bounded ambiguity candidates |
| `CancelCommand` | contextId string | versioned cancellation/stale result |

The live Voice1 unique owner must also have a current-owner, exact boolean true
confirmation of Settings1 `services.voiceInput`. A caller-supplied PID or provider
claim is not authority. The compositor must revoke before a later re-enable can
revive capture. This first transport admits the voice provider only. Shell operations retain
their existing panel-role credential join. Native launch receipts and
`group-tab`, `group-tile`, `launch` execution remain pending and are omitted
from capability replies. No deployed capability claim is made.

Requests have exactly four fields:

```json
{"version":1,"operation":"maximize","target":{"kind":"current"},"arguments":{"fraction":0.9}}
```

Targets are `current`, `window` or `container`. Named targets contain exactly one
`id` or `name`; current has neither. Names resolve uniquely or return explicit
ambiguity candidates. IDs are server-issued subject identities, not process IDs.
Requests are limited to 4096 bytes. Unknown fields, operations, argument keys,
invalid Unicode/control text and out-of-bounds numeric values are rejected.

| Operation family | Arguments |
| --- | --- |
| focus, raise, minimize, restore, close | none; close uses ordinary application close policy |
| shade, unshade, iconify, uniconify | none; container/window model ownership determines the valid subject |
| maximize | optional fraction from 0.1 to 1; default 0.9, full maximize 1 |
| fullscreen | required boolean enabled |
| rename, color | name up to 128 characters; color value in #RRGGBB form |
| place | region array [x, y, width, height] inside normalized usable output |
| detach, next-tab, previous-tab | none |
| group-tab | destination target |
| group-tile | destination target, direction right/down/left/up, optional ratio from 0.05 to 0.95 |
| activate-tab, reorder-tab | one-based index from 1 to 256, further bounded by live page count |
| resize-split | ratio from 0.05 to 0.95 |
| launch | installed desktopEntryId, placement tab/right/down/left/up/independent, optional destination target, region and containerName |

90% maximize uses 90% of both usable width and height, centered to leave 5% on
each side. The usable area excludes the top bar and other committed reservations.
Regions compose independently of utterance vocabulary: halves, thirds, quadrants
and explicit fractions use the same normalized geometry. Rounded edges stay
inside the area and adjacent equal fractional regions share an edge.

Status values are accepted, dispatched, ambiguous, stale, invalid, denied,
unavailable, cancelled and resource-limit. Accepted means a mutation committed.
Dispatched means an application launch was submitted, not that a window arrived
or placement succeeded. Launch authority and a bounded arrival/result contract
must be qualified before launch is exposed by the native adapter. Unsupported
capabilities never fall back to simulated keystrokes or typed dictation.

Gabbee retains configured custom-command precedence. Its composable parser must
separate operation, target, spatial/ratio modifier and installed application
selection, rather than limiting commands to a fixed list of example sentences.
See [Voice input](voice-input.md), [Hybrid topology](hybrid-topology.md) and
[Module boundaries](module-boundaries.md).

## Focused evidence and remaining gates

`qindaqt.window-management-geometry_codec` verifies top-panel/negative-origin
inset geometry, thirds/quadrants and malformed/unsafe requests.
`qindaqt.window-management-context_controller` verifies authority before lookup,
single use, owner/context replacement, expiry, lock revocation, stale subjects,
ambiguity without mutation, unsupported capabilities, cancellation and floods.
Those rows, the real private-bus `qindaqt.window-management-qt_endpoint`
owner/consent/replacement/lock scenarios, `compositor.semantic-fractional-maximize`
and six affected placement/preference/bridge rows pass with fatal Qt warnings. Private native scenarios pass six Qt checks per normal/actual-150% run, with 59 read-only scene snapshots each: ordinary raise/maximize/restore and thirds, original geometry after three repeated inset/fullscreen round trips, grouped rename/color/maximize/shade/restore/tab traversal/detachment, temporary icon/minimize restore, stale resize rejection and consent revocation. Actual DPR is asserted by the Qt client. The Gabbee command-hotkey source consumer is integrated (31147a55/eadb8995, manager88 tests and20 subtests); group/launch execution and installed-session qualification remain required before the feature is executable in the desktop.

An empty desktop can capture a context for a named target or independent app launch. Current-window operations still require a live normal subject at resolution; foreground changes after capture revoke that context. The platform scene currentness check scopes topology/geometry to the captured subject so mapping a provider-owned command popup does not invalidate its own hotkey.

Grouped fraction changes preserve the original restore rectangle, including roll-up/unroll and cancelled resize. Ordinary fractional maximize is owned by a small placement collaborator and refreshes when output/work-area reservations change. Provider popup exemption requires a Wayland surface whose kernel peer PID is the current Voice1 bus owner's PID; an X11 PID property is never enough. Scene currentness fingerprints only the captured subject geometry/state/container structure, so unrelated window mappings do not consume a valid hotkey.

Ordinary Wayland geometry acknowledgements may round server decoration/client boundaries by one physical pixel at fractional scale. Keep exact floating restore geometry, tolerate only that acknowledgement difference, and abandon inset state when the user starts moving/resizing. Native 150% acceptance must verify actual Qt DPR/output scale, not just a scenario filename.
