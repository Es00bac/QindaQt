# Network secret agent

`qindaqt-network-secret-agent` is the first-party interactive credential
boundary for NetworkManager. It is a separate process and module from
Network1, the NetworkManager adapter, and Settings. [ADR-0069](../adr/0069-confine-network-credential-entry.md)
records why credential entry is not added to any of those existing boundaries.

## Process and bus ownership

Production injects the real system-bus connection for NetworkManager and the
real session-bus connection for presence publication. Tests inject separate
connections to a private broker. The process exports only the standard
`org.freedesktop.NetworkManager.SecretAgent` interface at
`/org/freedesktop/NetworkManager/SecretAgent` and registers identifier
`org.qindaqt.NetworkSecretAgent1` through NetworkManager's
`org.freedesktop.NetworkManager.AgentManager.Register` method.

The session-bus name `org.qindaqt.NetworkSecretAgent1` is presence truth only:
it has no registered object path, methods, properties, signals, or credential
payload. The Settings Network route observes only ownership of that name. The
agent publishes it only after AgentManager registration succeeds and releases
it before registration is retired.

Every incoming method call is authenticated against the current unique owner
of `org.freedesktop.NetworkManager`. An owner loss cancels all prompts before
the owner is cleared; replacement registration completes before the presence
name returns. A delayed request from the retired owner therefore cannot send a
secret to its replacement. Startup remains resident while NetworkManager is
absent and registers when an owner appears. Bus loss terminates the process.

## Request admission

`GetSecrets` is fail-closed. The controller accepts a request only when all of
these facts hold:

- the caller is the current NetworkManager unique owner;
- the connection object path appears in that owner's bounded
  `Settings.ListConnections` result;
- the setting map, connection name and UUID, hints, and flags are bounded and
  structurally valid;
- the setting is `802-11-wireless-security` or `802-1x`;
- every requested hint maps to a supported field; and
- `ALLOW_INTERACTION` (`0x1`) is set and no unknown flag bit is set.

The 65,536-byte aggregate budget traverses supported nested lists, string
lists, maps, and hashes, counts container/key overhead and scalar payloads,
and permits at most 256 entries per nested container and eight nested levels.
An invalid value or any metatype the admission walker cannot account for is
rejected rather than treated as zero bytes.

The other recognized NetworkManager request bits are `REQUEST_NEW` (`0x2`),
`USER_REQUESTED` (`0x4`), and `WPS_PBC_ACTIVE` (`0x8`). They do not weaken the
interaction requirement. A non-interactive, foreign, unknown-connection,
unknown-setting, unknown-hint, malformed, or over-budget request returns
`org.freedesktop.NetworkManager.SecretAgent.Error.NoSecrets` without opening a
prompt.

For personal Wi-Fi the prompt requests only `psk`, or one exact
`wep-key0`…`wep-key3`. For 802.1X it requests only `identity` and/or `password`.
Hints narrow the response; they never expand it. Certificate and private-key
selection are outside this slice.

## Prompt and lifetime

One application-modal QindaQt.Controls/QST prompt exists per internal request
id. It shows the bounded connection name and requested field labels. Each
editor has a field-specific maximum, concealed fields have a show/hide toggle,
and the prompt exposes accessible dialog, heading, field, checkbox, and button
semantics. Enter submits, Tab and Shift+Tab use normal control order, and
Escape cancels.

`CancelGetSecrets`, Escape, window close, current-owner loss, process stop, and
the bounded 120-second default timeout all remove the prompt and complete the
delayed call once with
`org.freedesktop.NetworkManager.SecretAgent.Error.UserCanceled`. Late prompt
completion is ignored.

## Secret and storage contract

Secrets exist only in prompt editors, short-lived `QByteArray` values, the
standard method inputs, and the standard `GetSecrets` reply. The QML boundary
passes editor object references rather than constructing a JavaScript secret
map. C++ converts each editor value to UTF-8, overwrites the dynamically owned
shared UTF-16 allocation in place, and clears the editor before completion.
The reply contains exactly the requested setting and fields. This process has
no settings store, secret store, cache, QindaQt D-Bus credential API, or
payload logging.

All directly owned byte and UTF-16 allocations are overwritten without a
copy-on-write detach before release. The temporary reply map is overwritten
and cleared immediately after synchronous D-Bus serialization. Every
`GetSecrets`, `SaveSecrets`, and `DeleteSecrets` input map is recursively
overwritten on method return, including rejected calls, because the standard
inputs can contain NetworkManager-owned secrets. No `QString` secret is
retained by the process. Diagnostics contain fixed redacted text only.

The remember checkbox controls NetworkManager's standard per-secret flags:

| Choice | Reply flag | Storage truth |
| --- | ---: | --- |
| Remember checked | `0` (`NONE`) | `AGENT_OWNED` is clear, so NetworkManager may store the secret in the connection profile |
| Remember unchecked | `2` (`NOT_SAVED`) | `AGENT_OWNED` remains clear and NetworkManager must not persist the supplied value |

The agent never sets `AGENT_OWNED` (`1`), because it owns no storage. It also
never uses `NOT_REQUIRED` (`4`) for a field it requested. Authenticated
`SaveSecrets` and `DeleteSecrets` recursively scrub their input and return the
standard typed void acknowledgement as storage no-ops: NetworkManager owns any
remembered storage.

## Package and proof boundary

Production `qindaqt-session` starts the agent only when the executable is an
installed sibling. It is non-essential: startup and registration never block
session readiness, absence is silent unavailable truth, and an unexpected exit
is restarted once. Logout and notification-host failure still stop it during session teardown;
paced shell recovery leaves it resident. Ambient `PATH` is not searched, preventing an unrelated
development binary from being adopted by the production supervisor.

The `QindaQtNetworkSecretAgent` install component carries the executable, QST
theme, and the Tokens and Controls QML modules needed by the standalone
process. Its focused test matrix covers controller hostility, prompt keyboard
and accessibility behavior, a private-bus fake AgentManager and Settings
owner, owner replacement, standard method replies, presence observation,
relocated launch, and positive/poison dependency checks. See the [testing
harness](../development/testing-harness.md).

This boundary does not claim VPN secrets, certificate selection, profile
creation/editing, an agent-owned secret store, physical networking, systemd or
D-Bus activation policy, or autostart outside the QindaQt session supervisor.
