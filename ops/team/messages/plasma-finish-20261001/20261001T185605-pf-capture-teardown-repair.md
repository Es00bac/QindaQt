# Protected capture teardown repair and bounded rebuild request

Second bounded strict build source6df1606a65ada0a85d52cd5d66f4ee051b90dec8
exited1 in14s, raw build/6df1606a-build.log and status JSON retained on qinda.
Final96/207 is dynamic progress, not a completed-action total. Minimum available
RAM19251708kB; no memory/stall intervention. Owned compiler group exited and
compiler released before repairs. First433 failure remains intact too.

The owned nativeOwner unbraced same-line declaration caused strict indentation
failure. Equivalent adjacent guard formatting and explicit includes are repaired.
A separate source audit found NativeCaptureAdmission destructor's monitor.stop
publishes denial synchronously while the broker's earlier-declared admission
outlives its later job storage. It could reenter the subscriber after jobs unwind.
Admission teardown now stops its timer and disconnects monitor forwarding before
stop; ordinary live denial remains untouched. Public NativeLockStateMonitor,
transport and fork authority are unchanged.

Two added rows use an actual nonce receipt endpoint on the existing zero-service-
directory private bus and actual UNIX socket peer/PIDFD admission. They model
jobs-before-admission destruction with externally retained counters, proving no
callback can touch already-unwound storage, and require actual Locked denial to
forward while live. Existing authority-channel target adds the owned admission
sources/public SessionLockState dependency; no production testing hook and no
18th target. These rows are source only until a separate runtime grant.

Static evidence exit0: docs/navigation486, strict MkDocs, source boundary,
focused capture17/test14 shapes with zero skips/warnings, git diff check.
Same bounded17-target Ninja-j8-l24 scope and declared dependencies requested on
this frozen repair; global MAKEOPTS remains-j24-l24. No compiler/private slot
held and no CTests/native run claimed. Mixed development public headers remain
source-compilation-only; native row absent, selectors KDE, first slice and full
PF19 still unqualified. Root next action: exact repaired-source compiler grant.
