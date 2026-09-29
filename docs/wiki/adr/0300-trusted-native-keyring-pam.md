# ADR-0300: Authenticate native PAM token delivery through the system owner

- Status: Accepted
- Date: 2026-09-29
- Owners: QindaQt program manager and keyring module
- Scope: Native PAM source and synthetic/private qualification; no live stack edits

## Context

The native daemon exposes bounded unlock/rekey over its existing same-euid
Unix control socket. PAM tokens also authorize account access, so the socket's
UID and an executable inode alone cannot authorize their disclosure. A same-user
attacker can run the genuine executable with injected startup code. Numeric
PID lookup also permits reuse after a socket's credential process dies.
The program owner approved a small trusted activation owner rather than new
cryptographic attestation or a second authentication transport.

## Decision

Ship a root-protected fixed system qindaqt-keyring@NUMERIC_UID.service template
and sanitized launcher as Portage source inputs. PAM requests that exact unit
after pam_systemd has established the user's manager/runtime/bus. The system
manager must be UID0/PID1 at the fixed system-bus endpoint. Refuse transient
units, delegation, drop-ins, alternate fragments, writable/symlink ancestors,
non-numeric User identity and changed ExecStart arguments or paths.

Use a fixed protected helper, permanent identity drop and anonymous socketpair.
Borrow PAM tokens only to create bounded locked/wiped/DONTFORK owned pages;
send nothing until the helper acknowledges the trusted peer. SO_PEERCRED
identifies the UID/PID, while SO_PEERPIDFD pins the same kernel credential
process without a numeric-lookup race. Systemd GetUnitByPIDFD must identify the
exact unit/invocation; MainPID and active/running state must agree. Recheck the
same live pidfd and invocation before token forwarding. An existing ordinary
user activation remains prompt-only and is never killed or replaced.

Retain the public QKR1 protocol and durable-save acknowledgement. The daemon
authenticates the old password before rekey. Missing credentials, failure and
uncertain persistence never become a successful password-change result.
Login unlock errors remain nonfatal. No collection is silently created and
locked metadata never authorizes transport or secret disclosure.

Require Linux SO_PEERPIDFD (6.5+) and systemd GetUnitByPIDFD (261 on the tested
host). Missing support fails closed to native prompt fallback. Require Yama1–3
for the exec-to-main gap; core suppression/dumpable0 apply before token handling.
Root/CAP_SYS_PTRACE/trusted ancestors remain outside the ordinary-user model.

## Consequences

- Same-UID fake sockets and injected self-launches receive no PAM token. PID
  reuse/restart cannot retarget a held peer handle to another invocation.
- Packaging must reconcile ordinary user socket/service activation with direct
  system-unit startup and place session unlock after pam_systemd. An activation
  collision preserves the original owner and yields prompt fallback.
- Account password and keyring rekey are independent transactions. Optional PAM
  placement can permit an account change while reporting a keyring rekey error;
  the original keyring then needs its old password through the native prompt.
- Tokens are never argv/environment/files/logs. Owned pages wipe on replacement,
  consumption, UID change or PAM cleanup. PAM/kernel/framework copies have
  separate ownership; universal secure-memory claims are unsupported.
- Synthetic private tests replace only the manager authority port. Kernel peers,
  peer pidfds, actual daemon injection and PAM confdir behavior are executable.
  The production sd-bus/root service path is compiled, not activated here.
- PK4 extends native Passwords & Keys UI and trusted display/output selection.
  PAM observes/consumes existing tokens; it never opens its own password dialog.

The current lifecycle, ABI, verification and deployment limits are on the
[native keyring PAM bridge](../architecture/keyring-pam.md). Existing daemon
authority/storage contracts remain in
[ADR-0296](0296-native-keyring-daemon-boundary.md) and
[ADR-0292](0292-native-keyring-storage.md).
