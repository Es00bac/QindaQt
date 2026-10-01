# ADR-0330: Gate native source profile holds behind exclusive authority

- **Status:** Accepted
- **Date:** 2026-10-01
- **Owners:** Power1 service and native power policy
- **Supersedes:** None
- **Superseded by:** None
- **Builds on:** [ADR-0293](0293-settings1-native-power-policy-and-powerdevil-import.md), [ADR-0297](0297-preserve-power-preference-precision.md)

## Context

The [native power policy](../architecture/power-policy.md) calls for
source-specific profiles through Power1's existing holds. Settings1 already
stores those preferences, while packaged sessions still run PowerDevil. An
unconditionally enabled native writer would introduce two competing policy
authorities before the remaining lid, critical-battery and brightness cutover.

## Decision

The resident executable composes a narrow source-profile policy with its public
PowerServiceCoordinator and the public SettingsClient. The pure `power_policy`
import module retains its transport-free boundary. The policy never writes
ActiveProfile: it acquires one tagged profile hold and releases only that hold.
Confirmed AC, battery and low-battery facts choose the matching confirmed
`power.profile.*` preference. `none`, unknown source or authority, missing
capability, and an unsupported profile suppress acquisition and remove a
currently observed owned hold when release remains admitted. The upstream
[HoldProfile contract](https://upower.pages.freedesktop.org/power-profiles-daemon/gdbus-org.freedesktop.UPower.PowerProfiles.html)
admits only power-saver and performance. Balanced preferences are deferred:
release our hold without setting a base profile or claiming balanced applied.
Automatic balanced base policy needs a separate contract because an
ActiveProfile write cancels external holds.

Bare and packaged execution default to `--profile-policy=off`. Explicit
`--profile-policy=native-exclusive` declares the operator's exclusive native
cutover intent. A constructing-session-bus adapter subscribes before querying
`org.kde.Solid.PowerManagement`; only a successful current absence query admits
policy. Arrival revokes admission and releases the native hold; query or bus
failure is closed. The adapter never starts, stops or changes PowerDevil.
Final supervisor/packaging cutover and retirement are separate acceptance gates.

Each acquisition uses a fresh per-runtime nonce in its reason. Successful
operation completion must converge to an authenticated published hold before
replacement. Cleanup uses the observed exact handle, never another caller's
metadata or a retained stale epoch. A changed global Power1 epoch retires local
handles. Exact current-owner targeted ProfileReleased retires the stored cookie
and refreshes facts; unexpected cancellation suppresses reacquisition until a
new source/preference/authority input or explicit retry. Known refusals retry only after material admission/confirmed preference
changes or explicit local retry. Repeated equivalent facts never retry. A
bounded dispatch or observation timeout, malformed success or Uncertain result
quarantines new acquisitions for this runtime; late authenticated observation
may clean up the tagged hold only if the adapter recorded its cookie, but
never replays the acquire. A lost acquisition reply provides no cookie and
cannot authorize a fabricated release. An uncertain release fences that exact
hold against repeat dispatch even after preferences change. The provider adapter
bounds hold calls at three seconds and refreshes current-owner facts after
successful acquire/release.

## Consequences

- PowerService adds a public SettingsClient dependency, with borrowed
  same-thread lifetimes and no Settings transport private-header access.
- Existing external/user holds and Power1 v1 wire values remain intact. Idle
  inhibitor supported scopes remain zero.
- Real resident executable tests use injected UPower, profile and Settings1
  authorities on a daemon configured without host includes or activation
  directories; no host settings, buses or power action is used.
- This lands only the bounded PF2 source-profile slice. Lid policy, critical
  countdown, full idle consumers, PowerDevil retirement and physical/installed
  qualification remain separate work.

## Revisit when

The final native power cutover can remove the explicit dormant default after
all required policy consumers and installed session behavior are qualified.
