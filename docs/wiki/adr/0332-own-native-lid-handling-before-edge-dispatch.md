# ADR-0332: Own native lid handling before authenticated edge dispatch

- **Status:** Accepted
- **Date:** 2026-10-02
- **Owners:** PowerService and SessionActions
- **Supersedes:** None
- **Superseded by:** None
- **Builds on:** [ADR-0330](0330-gate-native-source-profile-holds.md), [ADR-0331](0331-fence-native-critical-battery-countdowns.md)

## Context

Power1 publishes lid/dock facts and Settings1 stores per-source lid preferences.
Native action policy must own the hardware event before dispatch, even when
PowerDevil is absent: logind otherwise handles the lid itself. Public Power facts
do not expose selected-session identity. A user-service process may belong to a
user manager rather than the selected graphical login session, so its PID or
environment session ID is not a safe selection shortcut.

The public Session1 current owner is the supervisor process. The constructing
session bus authenticates its UID/PID, and root-owned logind maps that PID to its
session. The existing protected public SessionActions boundary supplies lock,
suspend, hibernate and noninteractive power-off; no private Sleep or compositor
header is needed. Screen-off still lacks an admitted public action from its
Display owner.

## Decision

Add cohesive LidPolicy over borrowed same-thread public SettingsClient, current
Power coordinator facts, a narrow LidHandlingAuthority seam and dedicated public
SessionActions client. A confined platform adapter owns the exact-current
logind `handle-lid-switch` block FD. Separate `--lid-policy=native-exclusive`
defaults off and reuses canonical PowerDevil-name absence admission. No packaged
cutover, Power1 wire/schema value or idle inhibitor scope changes.

The adapter subscribes owner, selected-session Properties invalidation,
SessionRemoved and bus-disconnection before queries. It authenticates the current
Session1 unique owner through bus-daemon UID/PID, requires same-user Session1,
requires current login1 UID0 in production, calls GetSessionByPID with the
authenticated supervisor PID, and validates the exact `(uo)` User UID/path and
typed Active=true on the resulting bounded session path. Invalidation payloads
never grant authority; fresh current-owner and session queries do. Admission
reads through current owners and Active before FD acceptance/action submission.

The primary [systemd inhibitor contract](https://systemd.io/INHIBITOR_LOCKS/)
defines `handle-lid-switch` in block mode as ownership of low-level lid handling,
without inhibiting explicit sleep/power-off. The adapter retains one CLOEXEC
duplicate. Disable, owner/session loss, timeout and destruction close its own
descriptor; retired replies retain no descriptor. Unknown FD acquisition
quarantines that adapter lifetime and never automatically resubmits. No external
inhibitor FD or inventory is edited. A dedicated SessionActions stop synchronously
retires pending Can callbacks; dispatched uncertainty quarantines policy lifetime.

Source and dock facts select current `power.lid.{ac,battery,lowBattery}` action
or dockedAction. Low/Critical/Action select lowBattery only on battery. Preferences
must be Ready, current-owner and confirmed without pending refresh. None owns
lid handling but dispatches nothing. Suspend/Hibernate/Lock/PowerOff require
actual public action availability. Unsupported screen-off and unavailable actions
do not acquire/retain the lid-handling FD; native policy does not claim to apply
those choices. Screen-off awaits its owning Display boundary.

Only current confirmed Capability::Lid plus lidClosed=false arms an open baseline
after FD/action/Settings admission. The existing Power assembly publishes that
capability whenever valid SessionFacts exist; lidPresent separately becomes
proven after observing a close. Thus the first genuine open→closed event is usable
without a prior close/reopen, while actual closed dispatch requires lidPresent
proof. Baseline and dispatch share Power epoch, selected preferences and FD
generation. Startup closed, readiness, Settings changes or repeated closed facts
never manufacture a close edge. A real new open is required after consumed,
cancelled or rejected work. Reopen before dispatch cancels pending work.

## Consequences

- New platform integration is confined to the owning PowerService adapter;
  policy has no bus-daemon/login1 wire identity or private Sleep dependency.
- No owning public selected-session extension is needed: standard daemon
  UID/PID and authenticated logind lookup supply the explicit process boundary.
- Production main fixes UID0. A distinct noninstalled private resident compiles
  the same assembly with target-local constructor UID injection; no environment,
  CLI, global compile flag or installation entry can relax production credentials.
  The normal resident must reject the ordinary-user fake logind.
- Acceptance uses the actual resident/Settings1/UPower/public actions and real
  descriptor transfer with peer EOF, including late/timeout/owner-loss paths.
  All 34 authored lid behavior rows pass (36 Qt checks), along with the unchanged
  nine Power/SessionActions CTests (101 Qt checks). Separate genuine-first-close
  and descriptor-peer-EOF gates pass; every recorded private process/root is
  absent afterward and source/artifact hashes stay fixed. Independent review
  remains pending; no host lid, inhibitor, sleep/power-off, PAM or installed
  qualification is inferred.
- Profile/critical/Hibernate contracts remain unchanged. Screen-off, full lid
  preference coverage, full idle policy and final PowerDevil retirement remain
  separate PF2 boundaries; supported idle scopes remain zero.

## Revisit when

Display exposes an admitted public screen-off action and installed sessions can
complete the full lid matrix and final exclusive cutover.
