# ADR-0432: Review keyring requester before credentials

- **Status:** Accepted
- **Date:** 2026-11-02
- **Owners:** Platform / keyring
- **Amends:** [ADR-0296](0296-native-keyring-daemon-boundary.md), [ADR-0302](0302-native-passwords-keys-client-and-policy.md)
- **Superseded by:** None

## Context

The keyring helper received a caller identifier but discarded it and displayed
only a collection label above a focused password field. Agent work can launch
credential consumers indirectly, leaving the user unable to connect an unlock
request to an intended action. Standard Secret Service unlock is shared state,
not a grant limited to the initiating application, and matching-password unlock
can also unlock other collections (ADR-0343).

## Decision

Require an explicit request-review step before enabling any password or approval
control. Focus Deny initially. Identify the immediate process using bus PID/UID
and bounded operating-system executable/start-time observations. Capture at
admission and revalidate before helper launch and before using approval; missing
process identity disables approval. An unavailable executable remains explicitly
unknown and requires Continue without executable identity, since protected
applications can hide that path while retaining authenticated bus PID/UID and
observable start time. Up to six same-user parents provide clearly labelled
context. Do not inspect argv, environment, script contents or credentials.

Explain the actual daemon operation, target label, and approval scope. State that
an agent task/reason is unavailable when the API supplies none; never invent
one or treat a caller label as process identity. The portal's application ID is
separate reported metadata. Standard unlock explicitly discloses same-user
sharing and matching-password collection unlock. All dynamic text is plain text
with control and bidi-format characters escaped. Keep Deny reachable while long
request details scroll. Allow three minutes of helper lifetime and five minutes
including the queue, subject to shorter caller deadlines.

Agent workflow must also announce the specific tool, service, task reason and
approval scope before an operation can trigger authentication. Unattended work
returns action-required rather than opening an interactive credential prompt.
This supplies task context outside the dialog when the standard API cannot.

Replace private QMP1 metadata with bounded QMP2, carrying the additional typed
context. Deploy daemon, importer and helper together. Old/new mixtures fail
closed; they cannot downgrade to a label-only password prompt. Secret Service's
public method signatures and password-output frames stay unchanged.

## Consequences

This makes genuine keyring requests inspectable and requires deliberate review.
An executable path is not software attestation, observed ancestry is not agent
authority, and a script interpreter does not identify which script or task is
trusted. Processes able to share a bus socket or execute within an admitted
process remain inside the same-user trust boundary. Unlocked collections remain
shared; this decision does not add per-agent or per-secret access control. The
ordinary overlay cannot establish a secure-attention path or prevent another
client from drawing a lookalike window. Those require separate architectural
work and must not be claimed by this UI change.

Verification uses synthetic private-bus attribution/cancellation and native
operations, hostile metadata/control-character tests, process identity mismatch
and disconnect checks, and QML review/unknown-requester/focus/layout tests.
See [Native keyring daemon](../architecture/keyring-daemon.md#request-identity-and-informed-approval).
