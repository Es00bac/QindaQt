# ADR-0438: Authorize Link1 callers by executable

- **Status:** Accepted
- **Date:** 2026-11-02
- **Owners:** QindaLink desktop core (`src/services/link_service`)
- **Supersedes:** None
- **Superseded by:** None

## Context

`org.qindaqt.Link1` lives on the user's session bus, where every process of the
user can call it. Two independent reviews showed what that allows: sending SMS
silently (including to premium numbers), reading the whole SMS history (with
two-factor codes), answering phone notifications as the user, listing the phone
app inventory, and approving or removing pairings. The panel precedents
([ADR-0061](0061-authenticate-shell-window-actions-by-panel-owner.md)) and the
keyring ([ADR-0432](0432-review-keyring-requester-before-credentials.md))
establish the house approach: identify the peer from the bus daemon and `/proc`,
never from anything the peer claims. See the
[Link1 reference](../reference/link1-v1.md).

## Decision

1. **One policy.** `CallerPolicy` (link_service) implements the small
   `CallerAuthority` interface (link_transport). Every `Link1.*` adaptor takes
   one and calls `authorize(message)` first. A missing authority denies; the
   service refuses to start without one.
2. **Identity.** The sender's unique name is resolved to a PID with
   `GetConnectionUnixProcessID`. Identity is compared, never a path string:
   `stat("/proc/<pid>/exe")` follows the magic link to the real inode, and its
   device and inode must equal those of a trusted file, `stat`ed in the
   service's own namespace on every check (so a package upgrade is picked up).
   The caller's `/proc/<pid>/ns/mnt` must equal the service's own, which
   defeats `unshare -Urm` plus a bind mount over a trusted path (that makes
   `readlink` report the trusted path for a different file). The process start
   time (`/proc/<pid>/stat` field 22) is read before and after, and a change,
   a missing value, or an exe that reads ` (deleted)` denies.
3. **Trusted set.** `<bindir>/qindaqt-link` (the app) and
   `<bindir>/qindaqt-shell` (hosts the panel applet), plus any
   `--trusted-caller <exe>` on the service command line (repeatable). The
   option is test and development only: `--help` says so and the service logs
   a warning to stderr (the journal under the user unit) for every use. The set is never taken from a D-Bus method, a file
   or an environment variable.
4. **Classification.** Public metadata stays open: `Devices` (including `caps`),
   `PendingPairings`, `ListenPort`, `DevicesChanged`, `PairingRequested`,
   `UnreadCount`, `ThreadsChanged` (ids), `SendStatus`, `Revision`,
   `ReplyFinished`. Everything that reads content or acts as the user needs a
   trusted caller and otherwise fails with `org.qindaqt.Link1.Error.Denied`:
   `ConfirmPairing`, `Unpair`, `Ring`, every Messages method, and
   `Notifications.ListApps/SetAppMuted/Reply/Current`.
5. **No content on broadcast signals.** `CurrentChanged` carries only
   `{device,key[,removed]}` and the revision; trusted clients read the text with
   `Current()`.
6. **Send admission.** `Messages.Send` passes a `SendGate` after authorization:
   at most 5 sends per minute and 30 per hour per device
   (`Error.RateLimited`), and every accepted send posts the desktop notice
   "QindaLink sent a text to N recipient(s)" with the requester's executable
   name, through the existing `NotificationPresenter`. The rate limit applies
   even when no presenter is configured.

## What this does not stop

The policy raises the bar; it is not a boundary against code running as the
same user. A same-uid process can still:

- `ptrace` or otherwise inject into a running trusted client (subject to the
  platform's Yama setting), or edit a trusted binary it can write;
- start a trusted executable itself with `LD_PRELOAD`, `QT_PLUGIN_PATH`,
  `QML_IMPORT_PATH` or similar environment so that attacker code runs inside
  the trusted process;
- use file descriptors a trusted client leaks to a child;
- claim `org.freedesktop.Notifications` when no notification server is running,
  and so observe or fake the send notice and the pairing notification; and
- read the pairing code from the open `PendingPairings` method.

What it does stop is the casual case the reviews found: an unrelated program,
script or compromised web-facing helper calling Link1 as itself, including one
that tries a mount-namespace path trick.

## Coverage by default

`qindaqt.link-caller-coverage` introspects the running service and requires
`Error.Denied` for an untrusted caller on every method of every
`org.qindaqt.Link1*` interface that is not on its explicit open allowlist
(`Devices`, `PendingPairings`, `ListenPort`, `Revision`). A new member such as
`Link1.Setup` is covered automatically; adding to the allowlist is a review
decision.

## Send admission details

`Messages.Send` refuses ids that are not a known device (`Error.NotPaired`)
before any rate bucket exists, so the bucket table is bounded by the number of
real devices.

## Consequences

- A same-user process that is not QindaLink can no longer read or send
  messages or act on notifications. It can still see device metadata and the
  pairing code, which does not let it approve a pairing.
- Development builds must pass `--trusted-caller` for their binaries. The
  cross-implementation phone tests pass the `gdbus` executable.
- A process that can write the trusted executable, ptrace a trusted client or
  inject into it is out of scope: those are the same-user boundaries of the
  platform.
- Tests: `qindaqt.link-caller-policy` (decisions with fake process facts, rate
  limit, notice) and `qindaqt.link-caller-dbus` (the real service on a private
  bus with an untrusted and a trusted caller).

## Revisit when

Another trusted client appears (it joins the installed set), or the platform
offers a better caller attestation than PID plus executable.
