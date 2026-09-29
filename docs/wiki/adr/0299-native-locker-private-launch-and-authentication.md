# ADR-0299: private native locker launch and PAM approval

- **Status:** Accepted; executable slices require separate integration evidence
- **Date:** 2026-09-29
- **Owners:** compositor fork, native locker, lock authentication
- **Supersedes:** ADR-0294's executable-inode-only production binding restriction

## Context

A fixed root-owned executable is necessary but cannot establish native client
trust by itself: an independently launched copy can load caller-selected code
before main through loader variables. The PF7 private witness demonstrates this
with an actual harmless preload constructor. Authentication cannot be a boolean
supplied by QML, an application ID or a public D-Bus caller.

## Decision

The compositor creates the sole production locker socketpair and launches the
fixed root-owned executable with that owned descriptor. Only that exact client
connection can see the standard session-lock global. A self-launched copy,
even the same executable, cannot bind or unlock. The executable and fixed desktop
permission entry require root-owned, non-writable files and parent hierarchies.
The entry grants the standard interface through the public QindaQt permission
field; it does not supply a shell command, user identity or authentication result.

The launcher constructs a whitelist environment, omitting loader variables,
user Qt plugin and QML import paths, public display selection and platform
arguments. Native greeter code uses only compiled resources and root-managed
Qt/QML module paths. Theme and screensaver preferences select bounded native
values, never executable code or an arbitrary QML URL.

The compositor, greeter and native PAM worker disable core dumps and process
dumpability. Linux [execve](https://man7.org/linux/man-pages/man2/execve.2.html)
resets dumpability, so a pre-exec flag alone is insufficient. The launcher
requires [Yama](https://docs.kernel.org/admin-guide/LSM/Yama.html)
`ptrace_scope` 1–3 to protect its owned child during the exec-to-main gap; the
native child disables dumpability before consuming inherited transports.
Unavailable or unrestricted kernel protection fails closed. Both qinda and
qinda-top reported mode 1 in read-only checks. Root/CAP_SYS_PTRACE and trusted
launcher ancestors remain outside the untrusted ordinary-client boundary.
No product component changes kernel settings. Core protection intentionally
supersedes ordinary crash-dump/debug attachment for these authority processes.

PAM runs in a separate disposable native process with private inherited IPC.
The worker selects the real UID's session account and the fixed `qindaqt-lock`
service, rejecting caller-selected account/service and transport tokens from
another attempt. The channel uses a versioned fixed-size header, at most 4096
payload bytes and an absolute deadline; its native owner also terminates a
worker blocked inside PAM. Authentication success AND `pam_acct_mgmt` approval are required;
expired credentials, cancellation, malformed conversation, unavailable modules
or failure never unlock. Conversation prompts/responses are bounded; native
mutable response copies are wiped. Successful libpam response allocations belong
to libpam, and the disposable worker exits after its attempt. Secrets never enter
diagnostics or resident service state. After approved authentication/account
checks, the worker runs the optional keyring module's handle-bound
`pam_open_session`/`pam_close_session` notification and ends PAM before reporting
approval. This service stack performs only that protected handoff, not another
login/logind session. A keyring error falls back to a later keyring prompt and
cannot manufacture or revoke authentication approval. Cancellation during the
notification still prevents the worker from reporting authenticated unlock.

QML can present a prompt, submit a response or cancel. It has no success or unlock
method. Only the native controller consumes the owned worker's response and
requests protocol unlock, scoped to the current lock epoch and request. Stale,
cancelled, duplicate and account-failed approvals cannot unlock. The compositor
trusts this fixed native process boundary; it does not accept a public PAM claim
or pretend to independently authenticate another process.

## Backend seam and consequences

`org.qindaqt.KWin.NativeLock1` at `/org/qindaqt/KWin/NativeLock` admits
`RequestLock` and exports read-only `Locked` and `Protected` with change signals.
`RequestLock` reports launcher admission, never presentation or authentication.
`Protected` reflects actual current-generation physical output feedback even
without a locker, so PF8 can order suspend after a black fallback. There is no
unlock or authentication-result method. PF8 owns Lock1/ScreenSaver/logind policy.

Non-installed fixtures use a private compositor, synthetic preload witness,
temporary `pam_start_confdir` and a synthetic test PAM module. They never read
system PAM configuration or an owner's password, and are not a production
bypass. Installed greeter binding, owner-password authentication and physical
DRM remain separate held qualification gates. Packaging and PAM service delivery
remain manager-owned through Portage.

See [Native session locking](../architecture/native-session-lock.md) and the
[testing harness](../development/testing-harness.md).
