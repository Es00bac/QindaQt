# Native portal foundation

The resident composes independent standard Access, Notification, Email and
Inhibit backends alongside the unchanged [appearance backend](portal-service.md)
and separate [Secret backend](secret-portal.md). The source package declaration and selector route Settings, Secret, Access,
Notification, Email, FileChooser and AppChooser to QindaQt. Inhibit retains KDE fallback because native
Power consumes no complete idle scopes. Installed session and sandbox delivery
remain manager gates; no selector change claims physical qualification.

[ADR-0318](../adr/0318-native-portal-foundation.md) records this process boundary.
The unrouted [remote-input candidate](portal-remote-input.md) reuses these
request, session, consent and lock components for RemoteDesktop, InputCapture and Clipboard.
The [native chooser components](../reference/portal-choosers.md) adds independent
wire policy, adaptors and ordinary native-dialog helper under
[ADR-0322](../adr/0322-native-portal-choosers.md). Their actual private frontend/
native-input gate passed all methods, updates and lifetime scenarios before the
source family selector changed; no unavailable native chooser retries KDE.
The primary upstream contracts are the current backend XML for
[Access](https://github.com/flatpak/xdg-desktop-portal/blob/main/data/org.freedesktop.impl.portal.Access.xml),
[Notification](https://github.com/flatpak/xdg-desktop-portal/blob/main/data/org.freedesktop.impl.portal.Notification.xml),
[Email](https://github.com/flatpak/xdg-desktop-portal/blob/main/data/org.freedesktop.impl.portal.Email.xml), and
[Inhibit](https://github.com/flatpak/xdg-desktop-portal/blob/main/data/org.freedesktop.impl.portal.Inhibit.xml).
The installed XML signatures agree with those contracts.

## Public components

| Component | Responsibility | Borrowed boundary |
| --- | --- | --- |
| PortalRequests | Bounded delayed replies, standard Request.Close objects, actor and request lifetime | Constructing private/session bus and same-thread retirement callbacks |
| PortalSession | Native Portal1 selected-session attachment | Public CompositorAttachment and caller admission |
| PortalAccessPolicy | Typed question/choice validation and result validation | Core/DBus value types only |
| PortalAccess | Backend transport and one native consent helper | Request registry, session binding, public NativeLockStateMonitor |
| PortalNotificationPolicy | Typed notification and action projection | Core/DBus values only |
| PortalNotifications | Native notification forwarding, sealed raster decoding and action routing | Request actor registry and standard native Notifications service |
| PortalEmailPolicy | Bounded RFC 6068 draft URI projection | Core/DBus values only |
| PortalEmail | Standard request transport and native draft handoff | Public ApplicationUriOpener and readonly native admission |
| PortalInhibit | Standard request lifetime and native idle leases | Public PowerTransport, authenticated state receipts and readonly admission |
| PortalComposition | Explicit ownership and dependency construction | The foregoing public components; no policy or persistence implementation |

All objects and callbacks are confined to their constructing Qt thread. Public
headers document ownership, cancellation and teardown. Adaptors retire their
requests before borrowed ports are destroyed. The interface aggregate contains
no central behavior. Secret storage, source migration, power policy, Settings
presentation and compositor implementation remain in their owning modules.

## Actor and selected-session lifetime

Only the actual current unique owner of `org.freedesktop.portal.Desktop`, checked
against the bus daemon and same UID, may call backend methods or close Requests.
The frontend supplies authenticated, case-sensitive application IDs under the
standard contract. Empty host IDs are accepted for these four families; the
frontend remains the actor. This differs deliberately from Secret's refusal of
an anonymous host secret. No process name, shell Exec or application-supplied
identity establishes backend admission.

Request paths must have the standard desktop/request shape and bounded ASCII
components. There are at most 32 live Requests, 128 actor/application budget
keys, and 32 requests per key per minute. Close objects are registered before
side effects. Completion removes the object and timer before invoking retirement
callbacks. Owner replacement retires outstanding requests and budgets. Response
methods return standard `0` success, `1` cancellation or `2` failure, with results
only on success; malformed/unauthorized/capacity requests produce named errors.
Deadlines are bounded; no old request or lease is replayed into a new owner.

The native control object is `/org/qindaqt/Portal1`, interface/name
`org.qindaqt.Portal1`, with `AttachSessionWithDisplay(s basename) -> b`.
The first same-UID unique session caller is retained, including a failed attach.
A different caller cannot replace it. Public
[CompositorAttachment](compositor-attachment.md) proves the selected canonical
private ordinary socket peer/PIDFD joined to exact bus-daemon compositor owner
and PID plus retained selected-session liveness. This proves identity and
lifetime, not executable attestation. Frontend options cannot supply that
attachment. The supervisor's independent PortalSessionLifetime starts the optional native
backend after the shell, retains a dedicated selected-session connection and
supplies the canonical ordinary display basename. It uses bounded asynchronous
exact-owner/same-UID admission attempts, reattaches backend replacement on the
same session connection, and disconnects before stopping the child. Replies are
transport acknowledgements; the backend still owns actual peer/privacy admission.
Missing helper/display or failed attachment does not prevent login.

Access and Email start and publish only while that attachment is live and the
public exact-owner NativeLockStateMonitor admits Unlocked. Uncertainty, native
lock or attachment loss retires helper/URI requests. Inhibit additionally checks
the current authenticated Power1 receipt and releases owned leases on retirement.

## Access consent

`AccessDialog(o,s,s,s,s,s,a{sv}) -> (u,a{sv})` preserves the standard wire.
Title/subtitle/body are plain text with 512/1024/4096 code-unit limits. At most
16 choices and 32 options each are accepted, with unique bounded IDs, offered
initial values and a total choice text bound. Boolean choices return true/false;
other choices return only offered IDs. Successful results have exactly one
validated value per requested choice. Invalid helper output is failure.

The native QindaTK helper is a separate hardened process, at most one at a time.
Question input is bounded JSON on stdin; output is bounded JSON on stdout.
No user text or secret is logged. It receives a new owned ordinary Wayland FD,
with ambient DISPLAY/WAYLAND_DISPLAY removed and no pathname fallback.
The confined [foreign-parent adapter](../adr/0318-native-portal-foundation.md)
imports xdg-foreign-v2 handles. Consent is enabled only after the compositor
processes import and parent assignment; invalid/lost parent or timeout fails
instead of showing an unrelated unparented consent. An empty parent is an
explicit unparented request. Grant, deny, Close, frontend loss and native-lock
retirement all converge on the same token lifetime.

## Notification forwarding and actions

Backend version 2 forwards bounded native notifications with application/id
isolation, replacement and removal. Native IDs never identify another frontend
application namespace. Limits are eight active entries per application, 64
active globally and 32 pending native operations. Action and close signals check
actual current native-owner sender, interface, path and signature. Replacement
of the native service drops old IDs without closing a new service's reused IDs.
Frontend loss retires pending operations and closes its current owned entries.

Targets are bounded primitive values, strings, byte arrays or string lists;
other opaque containers fail explicitly. Non-`app.*` actions emit targeted
standard backend ActionInvoked to the frontend, preserving target and bounded
activation token. `app.*` follows the upstream
[GTK backend Application.ActivateAction convention](https://github.com/flatpak/xdg-desktop-portal-gtk/blob/main/src/notification.c)
for the frontend-supplied installed application ID. No arbitrary shell command
is constructed from a notification action.

Icons accept themed names and immutable sealed memfd PNG/JPEG content. The
platform decoder reads at most 4 MiB, 2048 pixels per dimension and two million
pixels, and transfers standard native image-data. Received FDs are released
after native transfer. Version 2 raw bytes icons, SVG and other unsupported
formats fail; arbitrary icon paths are refused. Sound accepts default/silent;
custom sound FDs fail. SupportedOptions is empty: purpose-specific buttons,
category and persistent/lockscreen policy are not advertised as implemented.

## Native Email handoff

`ComposeEmail(o,s,s,a{sv}) -> (u,a{sv})` projects addresses, cc, bcc, subject and
body into a bounded RFC 6068 mailto URI. Address fields refuse control characters
and commas; subject refuses newlines. Percent encoding preserves literal input
without allowing new header keys. Nonempty attachment lists fail explicitly:
portable mailto cannot promise attachment transfer.

The independent `services/application_uri` module borrows the public Default
applications store and ApplicationCatalog snapshot. It resolves only the
configured scheme handler and the public Launcher Exec grammar. Append-only
URL inputs support standalone `%u`/`%U`, exclusively with local-file inputs;
file or embedded URL codes cannot consume a URI. Terminal, D-Bus-only and
handlers without a URI field code fail explicitly. The Settings route remains
process-free and unchanged.

A one-use nonprivileged URI relay receives validated argv over bounded stdin and
a separately admitted ordinary display FD. It resolves no shell and does not
open an ambient display. Success means the native handler passed exec, not that
mail was sent. Cancel can retire a pending launch; after exec, an independently
running draft cannot be recalled. Admission is checked before dispatch and at
publication, so a retired request cannot receive successful results.

## Inhibit capability boundary

Standard flags are logout=1, user-switch=2, suspend=4 and idle=8. Only idle is a
candidate native mapping, and it requires all native idle stages (AutomaticLock,
DisplayOff and IdleSuspend, mask 7). Partial masks cannot claim a complete idle
inhibitor. Other flags, combinations and unavailable native scopes fail with
NotAllowed. The current producer advertises zero supported scopes, so the real
resident refuses every Inhibit acquisition. No successful inhibitor capability
or selector replacement is claimed from injected public-port tests.

A fresh authenticated Power1 state receipt precedes acquisition. Acquisition
has a four-second deadline; the backend void reply is sent only after a real
native lease. Its standard Request stays registered until Close, owner loss or
native retirement, and cancellation releases the owned lease. Late acquisition
results are released against their original owner without replay. CreateMonitor
returns failure and QueryEndResponse returns an explicit error because session
monitor/end-session behavior is not implemented. No fabricated Running state or
restore token is produced.

## Verification scope

The focused `qindaqt.portal-access`, `portal-notifications`, `portal-email` and
`portal-inhibit` fixtures run actual backend wire/lifetime behavior on private
real buses. Notification rows compose the production native notification host.
The Inhibit zero-scope row composes the production Power resident and transport;
positive lease lifetime rows use an injected public Power port.

`qindaqt.portal-native-consent` runs the production ordinary private compositor,
production consent controller/QML and test-only pointer input. It verifies actual
mapped consent grant/deny, offered choices, Close, frontend loss, invalid parent,
valid exported-parent consent and parent-loss retirement,
and actual native clientless-lock retirement. A synthetic installed mail handler
receives the literal encoded URI and verifies its ordinary FD's compositor peer
before recording a private mapped-window result. No host services or user data
are used. These rows prove private native functionality, not physical desktop,
installed selection, sandbox coverage, PAM authorization or trusted locker
unlock. See [Testing harness](../development/testing-harness.md).

The source qualification uses strict compiler warnings and fatal runtime warnings
for the new native-family gates. The adjacent historical Settings service test
contains an intentional wrong-signature ReadAll call: Qt emits its own
method-dispatch diagnostics, so that unchanged row runs with ordinary QtTest
warning handling. This does not weaken native-family runtime warning gates or
production warning behavior. The staged-package gate verifies the new helpers
and exact public headers while preserving Settings/Secret and all remaining
fallback/closed selector rows. The native frontend row composes the production
resident/foundation over synthetic appearance and notifications, calls the real
Camera.AccessCamera path into Access (the frontend has no Access interface),
Email.ComposeEmail and Notification, and observes native mapped input, URI peer
receipts and notification actions/removal. Public Close, frontend owner loss and
supervisor disconnect retire pending consent. The separate session lifetime
fixture verifies same-session attachment across distinct backend unique owners.
The staged-package row repeats native frontend positive/withdrawal controls using
installed metadata and URI relay. Consent input remains the explicit production-source
test driver, so this does not establish installed physical consent. These private
fixtures do not contact the host or install a portal.
