# PF19 protected capture admission and owned compile repair

- Worker: pf-portal-capture-sol-20261001
- Source inspected: qualified fork690, commit 690c0112d13ca7d861e070865c9054d657946b75; portal checkpoint f79512c485ca234877851f0ace6f0995694d8537
- Status: material source finding; predicted denial, not runtime reproduction
- Resources: compiler released after failing checkpoint; private capture runtime never granted or used

The third strict named-target rebuild exited 1, preserved in the isolated qinda
worktree `build/f79512c4-build.log`. Last Ninja progress was 44/60; that is not an
executed-action count. The sole compiler error was the owned policy assertions'
`qDBusMetaType` spelling. The corrected public spelling is `QDBusMetaType`.
Native helper, dialog and actual-input sources compiled in that invocation.

The production resident and native capture helper set `PR_SET_DUMPABLE=0`
before Qt/connections. Fork690 `src/utils/executable_path_proc.cpp:13` resolves
`/proc/<pid>/exe`; `utils/serviceutils.h:72` uses it for restricted Screenshot2
DBus permission, and `wayland/clientconnection.cpp:51` resolves a connection's
executable for `wayland_server.cpp:181` restricted-global filtering. An empty
path denies interfaces. Linux's protected proc-link access predicts that the
same-UID compositor cannot identify these protected processes. The source
fixture's previously dumpable resident could falsely qualify ScreenCast.
This is distinct from the deliberate legacy Screenshot2 service carveout.

The fixture now applies the same no-core/non-dumpable startup before Qt and
ordinary peers. The runner clears both inherited permission-bypass variables,
keeps ordinary peer/native receipts, isolates each process group and audits
postexit descendants. The app-ID desktop fixture uses a distinct executable so
restricted-interface lookup cannot accidentally choose an unprivileged duplicate
for the actual resident executable. No process protection, fork permission
check, accepted portal implementation, metadata or selector was weakened.

## Required public seam, for root coordination

Both restricted entry points need authenticated, interface-scoped grants that
work without proc-executable access: resident ordinary Wayland capture global
and native child DBus Screenshot2. A selected supervisor/compositor authority
must authenticate actual same-UID peer PID and live PIDFD, distinguish resident
and permitted child, and bind the grant to the exact current compositor owner
and existing ordinary connection. The child relationship must come from owned
process/FD authority, never a self-reported executable, app ID or PID string.

The grant's lifetime must remain subordinate to the current frontend/requester,
Request/Session and optional native parent; owner/PIDFD/parent loss, Close,
privacy uncertainty/lock and replacement must revoke it. A revoked grant cannot
be reused for another peer, connection, interface, capture or late result.
Read-through current native admission and compositor-side lock refusal remain
required; no ambient/root fallback or permission-bypass environment is allowed.
The minimal API may separate admission capability from portal operation policy,
but must preserve these contracts for both transports. Root acknowledged the
finding and will coordinate any separately owned fork/public-authority change.

Next: freeze/push this truthful fixture and spelling/isolation repair, request
the unchanged named-target compiler gate, then obtain a separately bounded
private runtime grant to reproduce denial before any new seam. No family
usability or PF19 completion claim follows from source or compilation.
