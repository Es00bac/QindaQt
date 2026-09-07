# Gabbee interoperability evidence

Bounded, executable evidence that the external dictation assistant Gabbee
(checkout at `/home/cabewse/gabbee`, read-only input to these tests)
interoperates with a QindaQt desktop: synthetic dictation into an ordinary
app and a terminal, focus capture including a grouped window member, and
GlobalShortcuts portal registration/routing.

The evidence assets live in `tests/session/gabbee/`.  Everything they do is
host-safe: no microphone capture (a fixed silent WAV replaces Gabbee's
recorder), no typing tool (PATH is stripped of `dotool`/`xdotool`), no uinput
device, and no synthetic input of any kind.  Gabbee's own mock STT provider
produces one fixed harmless transcript,
`[mock transcript from recording-1.wav]` in the controller-driven runtime.
The standalone recorder unit fixture uses `gabbee-synthetic.wav`; the runtime
verifies the controller’s numbered recording filename and requires a new AT-SPI
occurrence of Gabbee’s final formatted text.

## What is real Gabbee code and what is stubbed

| Layer | Evidence path |
| --- | --- |
| STT | Gabbee's `gabbee.stt.mock.MockSpeechToText` (fixed transcript) |
| Recorder | Probe `SyntheticRecorder` writes a fixed silent WAV; never opens an audio device |
| Focus capture/activation | Gabbee's `KWinWindowBackend`/`KWinQtScriptBridge` against the QindaQt compositor's stock `org.kde.KWin` Scripting service |
| Insertion | Gabbee's `TextDeliveryRouter` reaches AT-SPI `EditableText`; the probe requires a new post-delivery AT-SPI occurrence. Clipboard output is recorded as fallback evidence and can never make insertion pass |
| Global shortcuts | Gabbee's `PortalPushToTalkBinding` against a real `xdg-desktop-portal` frontend |
| Portal backend | Fake KDE backend claiming the reviewed selector's bus name, implementing the installed `org.freedesktop.impl.portal.GlobalShortcuts` contract |

The QindaQt side is always real: the production portal selector conf (a
runtime-staged copy), the real portal frontend, and — in the nested lane —
the real compositor, shell, Text Editor, and Terminal.

## Deterministic host rows (no display, no lane)

`session.gabbee-interop-unit` (skip code 77 when the Gabbee checkout or
`xdg-desktop-portal` are absent) covers the probe contracts — synthetic WAV
fixity, portal-conf staging, evidence schema, host-endpoint hygiene,
injection-free sources — and executes the full GlobalShortcuts chain:

```
private dbus-daemon (config with ONE probe-owned service dir)
  -> staged portals.conf copy adding org.freedesktop.impl.portal.GlobalShortcuts=kde
  -> real /usr/libexec/xdg-desktop-portal
  -> fake backend owning org.freedesktop.impl.portal.desktop.kde
  -> Gabbee's real PortalPushToTalkBinding
```

Assertions: both F5/F6 shortcuts register ("Hold F5 anywhere to talk."), a
backend-emitted `Activated` reaches Gabbee's pressed callback through the
frontend, `Deactivated` reaches released, and the backend journal records
exactly one `CreateSession` + `BindShortcuts` with Gabbee's shortcut ids.

AGENT-GUARD: the private bus config exists because a stock session bus
D-Bus-activates the host-installed real KDE backend from
`/usr/share/dbus-1/services`, which would silently steal the reviewed
selector's bus name.  The staging helper also refuses to run passthrough if
the integrated conf ever routes GlobalShortcuts anywhere other than the
reviewed `kde`-only line (accepted portal candidate 8215a8cd; see
[ADR-0059](../adr/0059-route-unimplemented-portal-families-explicitly.md)
family policy) — the evidence can never widen
routing on its own.

The fake backend implements the installed impl contract exactly:
`CreateSession(o,o,s,a{sv})`, `BindShortcuts(o,o,a(sa{sv}),s,a{sv})`,
`ListShortcuts(o,o,a{sv})`, and `Activated`/`Deactivated` signals whose
session handle is an object path — the frontend drops string-typed handles
silently, which is easy to miss when writing a fake.

## Nested lane row (root-owned private runtime lane)

`tests/session/gabbee/run_gabbee_interop_nested.py` boots the contained
desktop exactly like the `desktop.virtual` rows — bubblewrap namespaces,
private bus, nested KWin `--virtual` with a test scenario (which also enables
the compositor's development control surface) — plus read-only mounts for the
Gabbee checkout and a privately installed `Terminal` component stage.  It
then runs:

1. the same GlobalShortcuts portal chain against the nested session bus;
2. `gabbee_interop_probe.py`: Gabbee activates the Text Editor window and
   the probe focuses its visible content through public AT-SPI (a newly mapped
   window may initially focus its tab strip), then Gabbee captures the target, dictates the synthetic transcript, and the delivery
   result plus pre/post AT-SPI and clipboard readback are recorded; the phase
   passes only when Gabbee reports `at-spi` delivery and the post snapshot has
   a new transcript occurrence. The same check runs for the Terminal window;
3. the compositor's `DockWindows` development API groups editor + terminal
   into one window container; Gabbee must capture exactly the focused member
   (distinct `window_id`s) and deliver per member.

Both the nested interop runner and the Terminal PTY proof hand KWin the
standard generated private-session wrapper. It invokes the staged session with
`--no-polkit-agent --no-powerdevil`: `/usr` remains mounted for the real
Gabbee, portal, and compositor dependencies, while optional host provider
defaults cannot register on the proof's private bus. The wrapper leaves the
session, shell, and all Gabbee-facing application behavior intact.

The runner writes each child’s output directly to its log file. Gabbee’s
clipboard fallback may fork `wl-copy`; that process must not keep a captured
stdout pipe open after the probe exits. Each step has a 120-second deadline,
and the private namespace owns clipboard-process teardown. `DockWindows`
replies are decoded from the public `ay` payload, including dbus-python’s
array-of-bytes representation. KWin’s brace-wrapped UUIDs are normalized to
Compositor1’s plain window IDs. Content discovery associates descendants with
their application PID; descendant accessible objects need not repeat that PID.

The inner session sets `QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1` and installs the
standard `org.a11y.Bus` activation entry on its private session bus. A
preflight row records the private bus path, AT-SPI library availability, and
broker introspection result. If the Qt AT-SPI bridge is missing or the focused
control does not expose `EditableText`, the phase remains red and reports that
fact; clipboard contents do not mask it.

Typed-character insertion is absent from *this* probe: the compositor's
development input injector has a closed key enum with no character keys, and
uinput is unreachable by sandbox design.  Gabbee's production recovery chain
therefore proves the AT-SPI EditableText and clipboard paths here.

Typed insertion into a live terminal does **not** require a host-uinput lane.
An earlier revision of this page claimed it did; that claim was wrong and is
corrected here.  The supported route is the standard
`org.freedesktop.portal.RemoteDesktop` portal: the user approves one real
consent dialog, and the installed `qindaqt-agent-input` helper — which
Gabbee's own `AgentInputTextSink` drives — delivers keysyms through that
approved session.  No uinput device is created, no typing tool is admitted,
and no permission check is bypassed.  That route is tested separately by
[the Terminal PTY proof](#terminal-pty-proof), which reads the delivered
sentinel back out of the terminal's own shell.  This probe keeps its
no-synthetic-input contract because it is a different, narrower experiment,
not because typed insertion is impossible.

## Terminal PTY proof

A separate row tests what the probe above deliberately does not:
that Gabbee's own text sink can type into a live Terminal through the real
RemoteDesktop portal, verified by reading the text back out of the
terminal's own shell rather than out of an accessibility API.

Its assets are decomposed by the boundary each one owns, so no module
carries unrelated behaviour:

| Module | Owns |
| --- | --- |
| `gabbee_terminal_sentinel.py` | The sentinel and its exact expected bytes. Pure; both delivery and readback derive from it so a delivery bug and a readback bug cannot cancel out. |
| `gabbee_terminal_sink.py` | The adapter around Gabbee's real `AgentInputTextSink`, including the concurrent-approval drive. |
| `gabbee_terminal_lane.py` | Outer bubblewrap lane: acknowledgement, session lock, staging, sandbox spec, evidence collection. |
| `gabbee_terminal_boot.py` | Inner session boot and teardown, in the order the portal requires. |
| `gabbee_terminal_delivery.py` | Portal lifecycle, the two delivery routes, and the byte-exact readback. |
| `gabbee_terminal_portal.py` | Real-portal helpers: window enumeration, dialog approval, PipeWire/screencast readiness, docking. |
| `run_gabbee_terminal_pty_proof.py` | Entry point only. |

### Why two delivery routes

Gabbee's `AgentInputTextSink` is a *text* sink: `deliver_key()` reports
failure by design.  The proof therefore splits delivery, and the split is
what makes it an integration proof rather than a helper proof:

- **The sentinel** — the payload the proof is about — is typed by Gabbee's
  real sink, using the sink's own helper lifecycle and its own
  `{"requestId": ..., "ok": true}` acknowledgement contract.
- **Framing only** — the pointer click that focuses the Terminal and the
  Return/Ctrl-D strokes that open and close the `cat` redirection — comes
  from a separately approved direct helper session, because the sink offers
  no key-chord route to carry them.

A unit test pins the sink's key-chord refusal, so if Gabbee ever gains a real
key route that test fails and the framing session should be retired.

### Three load-bearing runtime constraints

Each of these was found from a portal denial that reports itself only as a
one-line warning in the KDE backend's log, so each is stated as an
`AGENT-GUARD` at its call site:

1. **Service cache.** KWin grants `zkde_screencast_unstable_v1` only to a
   client whose desktop file it can resolve through KApplicationTrader.  The
   fixture writes an XDG applications menu and runs `kbuildsycoca6` before
   the backend starts; without it the interface is withheld and every
   `CreateSession` is denied.
2. **Latched probe.** `xdg-desktop-portal-kde` probes that interface once at
   startup and latches the result, so a backend that starts before the
   global appears stays broken for its whole lifetime.  The proof restarts
   the backend and retries, bounded, rather than depending on timing.
3. **Parent compositor.** The child KWin must run `--windowed` on a private
   Weston parent (`--backend=headless --renderer=pixman`).  The `--virtual`
   platform never advertises the global, and forcing `KWIN_COMPOSE=O2` there
   is a no-op — it logs `Configured compositor not supported by Platform`.
   Weston renders in software, so this is **not** a GPU requirement.

The Weston prefix reaches only its own `LD_LIBRARY_PATH`, never the
sandbox-wide search paths, or the system KWin loads a private-prefix libkwin
and rejects the release-matched plugin.

### Rows and invocation

`session.gabbee-terminal-pty-unit` covers the sentinel contract, the chooser
predicate, and the sink adapter; it is host-safe and needs no lane, and its
two real-sink contract cases skip when the Gabbee checkout is absent.  The
sandboxed proof is manager-run and deliberately not a registered CTest row,
because it needs the private runtime lane:

```sh
QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop \
python tests/session/gabbee/run_gabbee_terminal_pty_proof.py \
  --build-root <configured-build> \
  --kwin-wayland /usr/bin/kwin_wayland \
  --plugin-relative lib64/qt6/plugins/kwin/plugins/qindaqt_compositor.so \
  --decoration-relative lib64/qt6/plugins/org.kde.kdecoration3/org.qindaqt.so \
  --gabbee-root /home/cabewse/gabbee
```

It additionally needs the private Weston prefix (`--weston`, defaulting to
the accepted audit configuration's binary).  Both the standalone Terminal and
the Terminal grouped with the Editor through `Compositor1.DockWindows` are
tested, each with its own sentinel and output file, so neither case can be
satisfied by the other's artifact. The earlier direct-helper revision passed
both cases twice. The integrated Gabbee-sink revision passes live runs
`bd2ad5a9fa424203ab552f7075a90834` and `6fbbe1a2f3f0485880eb4925cf057cf8`: each
has 11 passing phases, the real external sink class and successful delivery
results, separate standalone/group sentinels, and byte-exact files independently
checked by the manager. Both private sessions clean up completely.

## Host AT-SPI bridge prerequisite

Live AT-SPI is unavailable when Qt Gui was built without its AT-SPI bridge. On
the qualification host, Qt lacked that bridge. Enabling it required a coherent
matching-version rebuild of the installed package set:
`qtbase[accessibility]`, `qtdeclarative[accessibility]`, `kwin[accessibility]`,
and `kwin-x11[accessibility]`; Portage adds `libqaccessibilityclient`. The
installed KWin variants depend on Qtbase with the same USE value, so rebuilding
Qtbase alone is not a valid transaction. `at-spi2-core` and
`QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1` cannot provide a bridge that was omitted
from Qt at build time.

Package mutation and session restart are host operations, not QindaQt source
evidence. After the matching rebuild and a fresh session, confirm the enabled
Qt bridge and AT-SPI broker, then run the manager-owned nested Gabbee proof
that requires an `EditableText` node. Library presence or clipboard fallback
alone never qualifies live accessibility.

## Exact invocation (root, with the private-runtime lane allocated)

```sh
QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop \
python tests/session/gabbee/run_gabbee_interop_nested.py \
  --build-root <configured-build> \
  --kwin-wayland <qindaqt-kwin-wayland-binary> \
  --plugin-relative <kwin-plugin-relative> \
  --decoration-relative <decoration-plugin-relative> \
  --gabbee-root /home/cabewse/gabbee
```

Preconditions: the desktop stage fixture
(`ctest --test-dir <build> --fixtures-setup desktop_virtual_stage`), a
configured build with the `Terminal` install component, `bwrap`, and the
Gabbee checkout with its venv.  Missing lane acknowledgement exits 77 before
touching anything.  Results land in
`<build>/tests/session/gabbee-results/<run-id>/`.
