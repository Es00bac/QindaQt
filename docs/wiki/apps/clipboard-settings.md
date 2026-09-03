# QindaQt Settings — Clipboard route

`qindaqt-settings --page clipboard` is the first-party control surface for
QindaQt's private, volatile clipboard history. It composes only the public
Settings1 and Clipboard1 clients. The page can change the history preference,
show bounded service metadata, and clear all current history after explicit
confirmation. It has no authority to read, preview, paste, copy, select, or
delete individual entries.

The resident-service boundary is fixed by
[ADR-0058](../adr/0058-isolate-clipboard-capture-in-a-volatile-host.md),
[Clipboard service architecture](../architecture/clipboard-service.md), and
the [Clipboard1 reference](../reference/clipboard1-v1.md).

## Preference truth

The route scopes an independent public Settings1 client to
`services.clipboardHistory`. Both schema v1 and v2 define only this Boolean
clipboard preference, with default `false`; no retention-size or duration key
exists, so this route does not invent one. The page says plainly that history
is off by default. When enabled, the resident service may hold recent text and
image selections in volatile memory, subject to Clipboard1 privacy gates and
the protocol's fixed capacity. Turning the preference off causes the resident
service to purge history under the service contract.

The saved value and the user's draft remain distinct. Apply sends one
optimistic Settings1 commit against the exact owner, epoch, and revision.
Success remains Saving until a subsequent authoritative snapshot confirms the
value. A conflict preserves the draft and offers an explicit “Apply my choice”
action; cancellation restores the current confirmed value. Unknown commit
outcomes and authority replacement preserve the draft, label the result
uncertain/not replayed, and never retry the mutation automatically.

## Service metadata and clear admission

The route model borrows one same-thread public `ClipboardClient`. It projects
only these values to QML:

- available, degraded, or privacy-denied state;
- history-enabled and privacy-admission truth;
- entry count and the protocol capacity of 64; and
- the current owner-fenced epoch, generation, and revision numbers.

Clipboard entry descriptors, identities, formats, previews, fingerprints, and
payload bytes never cross the route-model boundary. The page consequently
cannot become a clipboard viewer through ordinary QML edits. An allow-list
include scan also rejects private service/Wayland APIs, payload decoding,
`QClipboard`, `QMimeData`, and per-entry copy/select/remove/read intent.

“Clear history” is admitted only from a current public snapshot reporting
history enabled, privacy allowed, and at least one entry, with no operation
pending. Opening confirmation captures the exact client owner plus snapshot
epoch, generation, and revision. Confirmation rechecks that complete lineage
before sending the public all-history clear request. A changed snapshot closes
the confirmation rather than applying against stale truth. Submitted clears
remain pending until the exact operation result and an authoritative snapshot
at or beyond its observed revision converge. Rejection is failed truth;
transport loss, malformed/inexact completion, or authority loss is uncertain
truth and is never replayed.

## Composition, interaction, and accessibility

The QML module owns a route-local composition singleton containing independent
Settings1 and Clipboard1 public transports and clients. Their request-token
domains are not shared with another Settings route. The model borrows both
clients, and both outlive it. Only the model QObject is exposed to the page;
D-Bus is confined to the composition source.

The page uses QST-1 roles and `QindaQt.Controls 1.0`. At widths below 560
logical pixels its service cards form one column; wider pages use two columns.
The content scrolls with Page Up/Page Down and Ctrl+Home/Ctrl+End. Controls
expose accessible roles, names, descriptions, checked/busy/disabled truth, and
visible status text rather than color-only state. Clear requires an explicit
modal confirmation with Cancel/Clear buttons and Escape dismissal.

The declared first-focus target is Close. It is enabled under loading,
degraded, privacy-denied, conflict, pending, failure, and uncertainty, so both
wide and compact Settings hosts always have an admitted Tab target. Escape
returns focus to the active route tab. `Ctrl+9` selects Clipboard, leaving the
reserved `Ctrl+8` slot for Power.

The `SettingsAppearanceRuntime` install component carries the Clipboard QML
module and the statically composed Settings executable. Relocation tests run
the installed executable from a sanitized stage while build-tree QML remains
present, with host display variables removed and both D-Bus addresses pointed
at nonexistent sockets.

## Verification and stopping point

Focused Clipboard selection:

```sh
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir build/dev --output-on-failure --no-tests=error \
  -R '^qindaqt\.settings-clipboard-'
```

The six rows cover Settings1 draft/apply/conflict/uncertain/replacement truth,
Clipboard1 metadata projection and exact-lineage clear admission, warning-fatal
wide/compact page construction and accessibility, positive and hostile
boundary scans, and staged relocation. Settings Center's focused selector adds
the typed route order, `Ctrl+9`, tab accessibility, Escape/Tab entry in both
layouts, root construction, and the common installed package.

All model tests use injected fake transports. Product and test runs do not
contact a host session bus, system bus, compositor, data-control server, or
host clipboard. This slice does not claim clipboard content presentation,
individual-entry actions, configurable retention bounds, persistent history,
live Wayland capture, live AT-SPI/screen-reader traversal, hardware input, or
nested-session screenshots.
