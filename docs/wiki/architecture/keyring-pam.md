# Native keyring PAM bridge

The native source module provides login unlock and authenticated password-change
rekey through the daemon's existing bounded control socket. It never accesses
storage internals, creates a collection, reads another provider's wallet, or
changes an installed PAM stack. Packaging and live deployment remain separate.
See [ADR-0300](../adr/0300-trusted-native-keyring-pam.md),
[native daemon](keyring-daemon.md) and [storage](keyring-storage.md).

## Token and error contract

Place the optional authentication observer after the real authenticator.
It returns PAM_IGNORE and cannot authenticate a login. PAM_AUTHTOK is borrowed
only while copying at most4096 nonempty bytes into locked, zeroing, DONTDUMP and
DONTFORK pages. PAM owns its original pointer; the module cannot clear it.
Each PAM handle fixes the resolved UID/GID. Identity changes retire cached
tokens. Replacement, session consumption, close-session and pam_end retire
owned pages. The module never conducts a password conversation. pam_setcred returns
PAM_IGNORE. Native lock reauthentication may call pam_open_session only after
successful pam_authenticate and pam_acct_mgmt, then close-session/pam_end;
this consumes the same token without a new greeter transport.

Place optional open-session after pam_systemd has made the user's manager,
runtime directory and session bus available. Unlock failure, mismatch, missing
token, unavailable daemon or unsupported trust infrastructure never prevents
login: open-session returns PAM_SUCCESS and leaves native prompt fallback.
An existing unlocked collection still requires password authentication; a
wrong password retires its decrypted state. A rejected KDF admission cannot
authorize new disclosure and can require a later prompt/retry.
After an authenticated `login` unlock, the daemon can asynchronously try the
same token against other locked persistent collections. Each store verifies
its own password, and the PAM reply still waits only for `login`. See
[ADR-0343](../adr/0343-unlock-matching-keyring-collections-with-one-password.md).

Password PRELIM_CHECK captures PAM_OLDAUTHTOK; UPDATE_AUTHTOK requires both old
and new nonempty bounded tokens. The daemon authenticates old before deriving
new, even when already unlocked. Missing tokens return PAM_AUTHTOK_RECOVERY_ERR;
rejected authentication, absence, timeout or failed/uncertain durable save
return PAM_AUTHTOK_ERR. Only the daemon's durable acknowledgement becomes
PAM_SUCCESS. The PAM deployment must choose optional-stack error behavior:
account-password changes and encrypted-keyring rekey are separate transactions;
failure does not roll back an account password already changed by another module.

## Trusted activation and peer identity

A merely same-UID socket or root-owned executable pathname is insufficient
authority for a login token. An attacker can self-launch the actual binary with
injected startup code. Production first asks the root system manager to start
exactly qindaqt-keyring@NUMERIC_UID.service using job mode fail. A non-root
lock worker may read/accept an already active configured system owner, but never
requests privileged startup or a polkit conversation. It never kills or
replaces an existing user-activated owner. Name/socket/writer collisions fail
closed to prompt fallback.

The fixed system-bus address ignores caller environment. System-manager
credentials must identify UID0/PID1. The exact nontransient, nondelegated unit
must declare the numeric User, no drop-ins, the packaged FragmentPath and one
fixed ExecStart launcher with --uid NUMERIC_UID. The fragment, launcher and
daemon, including every ancestor, must be root owned, non-writable by group or
others, and free of symlink traversal. Untrusted runtime overrides do not exist
in the production module.

A fixed root-protected helper is execed with only PATH/LANG, after permanently
dropping supplementary groups and all real/effective/saved UID/GID to the PAM
identity. No token travels in argv, environment or a file. Anonymous socketpair
descriptors carry readiness and the existing QKR1 frame. Secure token mappings
are absent from the fork child before exec. Independent PAM heap copies can
still be inherited: the child suppresses dumps before and after identity drop,
and exec retires that address space before any new token is sent. The helper first checks the private
0700 runtime and owned0600 socket through a pinned no-follow directory
handle, then kernel SO_PEERCRED and SO_PEERPIDFD.
SO_PEERPIDFD binds the pidfd to the actual socket credential process; looking
up a numeric PID afterwards would leave a reuse gap.

Systemd GetUnitByPIDFD(h) must map that held kernel process to the exact numeric
unit and a nonzero16-byte invocation. Unit MainPID must equal the kernel peer,
with active/running state. The helper checks pidfd liveness and repeats the
manager/invocation checks immediately before forwarding tokens. Restart,
disconnect, stale/dead credentials, fake same-UID peers and injected self-launches
cannot substitute another owner. Loaded locked search metadata never supplies
any part of this authority.

This requires systemd with GetUnitByPIDFD (version261 on the qualification host)
and Linux SO_PEERPIDFD (introduced in6.5). Missing support denies automatic token
delivery; login still succeeds. Yama ptrace_scope1–3 is required for the
exec-to-main interval: exec resets dumpability. The PAM parent verifies this
policy before fork/delivery so a hijacked gap cannot fake helper readiness.
Helper/launcher/daemon suppress
core dumps and set dumpable0 before handling tokens. Root, CAP_SYS_PTRACE and
trusted process ancestors are outside this ordinary-user threat model.

## Lifecycle and packaging boundary

The source system template uses User=%i, a fixed protected launcher, no
delegation, core suppression, restricted address families and an environment
without loader/QML overrides. The launcher constructs a minimal environment
from the account record and /run/user/UID, then execs the fixed native daemon.
No caller environment supplies a plugin, library, bus or executable path.
The user manager must already exist; the system template binds its lifetime to
user@UID.service and does not restart a detached broker.

PK2 user service/socket and D-Bus activation remain ordinary prompt-only
activation. The system template starts the daemon directly rather than taking
over the user socket unit. Packaging must select coherent activation ownership
before deployment: if ordinary user activation wins first, automatic login
unlock is refused and the native prompt remains usable. The QindaQt supervisor
can attach to a trusted system-started daemon through the existing unique-owner
session boundary; its disconnect terminates and wipes the daemon.

CMake provides the QindaQtKeyringPam component with the module, helper, launcher
and system template. This is a Portage packaging input, not an installed stack.
A future ebuild must own source installation, exact paths, stack ordering,
system activation permission and activation-file reconciliation. Tests do not
install or start any live unit. The system launcher clears inherited display
metadata and requires a validated session display attachment before a prompt.

The supervisor supplies its current native socket basename through additive
Keyring1.AttachSessionWithDisplay(s); legacy AttachSession remains compatible
but supplies no display authority. CompositorNames owns the qindaqt- prefix.
Only canonical numeric slots0–4095 are admitted, beneath the pinned no-follow
owned0700 runtime directory. Admission joins the unique session owner and
active native compositor bus owner/PID/UID to the actual Unix SO_PEERCRED and
SO_PEERPIDFD. A compositor-name change, dead peer, session disconnect or socket
lineage loss revokes approval. Each helper inherits the exact newly connected,
validated ordinary Wayland FD as WAYLAND_SOCKET; it never reconnects by pathname.
A replaced pathname cannot redirect that connection. No locker-only connection
or privileged locker capability is shared.

This display binding authenticates the declared current compositor lineage, not
its executable or a PAM token recipient. The separate protected system owner
remains the only PAM delivery authority. Native rendered overlay/multi-output
qualification and best-effort cursor output selection remain bounded PK4 gates.

## Verification and memory limits

keyring_pam_integration uses pam_start_confdir with temporary noninstalled
synthetic modules, private buses, real native children and real Unix peer
credentials/pidfds. It covers unlock, already-unlocked authentication, restart
after rekey, wrong/missing/oversize tokens, failed locked-page allocation without stale
credential reuse, identity change, failed durable save,
absence/readiness bounds, unsupported parent Yama policy, retained dead-peer
sockets, unavailable SO_PEERPIDFD,
same-UID zero-payload rejection, injected actual-daemon rejection and competing
ownership. A separately compiled manager seam pins the sanitized child created
by the fixture; no such runtime option exists in production.
keyring_pam_owner_policy checks manager identity/state refusals and protected
path permissions/symlinks. keyring_prompt_display checks actual private bus
and kernel peer lineage, native slot/FD environment, fake or replaced sockets,
owner replacement, in-flight cancellation, disconnect and default cleared state.
The existing Secret Service suite preserves legacy attachment/prompt behavior.
The production sd-bus transport is compiled against
installed systemd; real root system activation remains a deployment gate.

Owned token/frame pages are locked and wiped. Anonymous socket/kernel buffers,
PAM's borrowed tokens, framework allocations and independent application copies
are not universally secure memory. No token is logged, put in error text or
saved for a later session. The daemon's storage/index leakage limits still
apply; this bridge adds no encryption-key retention.

Primary contracts are documented by
[Linux-PAM pam_get_item](https://github.com/linux-pam/linux-pam/blob/master/doc/man/pam_get_item.3.xml),
[Linux-PAM pam_sm_chauthtok](https://github.com/linux-pam/linux-pam/blob/master/doc/man/pam_sm_chauthtok.3.xml),
[systemd D-Bus API](https://github.com/systemd/systemd/blob/main/man/org.freedesktop.systemd1.xml),
[Linux SO_PEERPIDFD definition](https://github.com/torvalds/linux/blob/v6.5/include/uapi/asm-generic/socket.h),
[Yama](https://docs.kernel.org/admin-guide/LSM/Yama.html) and
[execve](https://man7.org/linux/man-pages/man2/execve.2.html).
