# Native sleep implementation checkpoint

Worker pf-power-sol-20261001. Exact base 53bf486b; ownership includes new src/session/native_sleep and tests/session/native_sleep, focused native_lock_runtime APIs, SessionActions client/availability/suspend helpers/tests, and coordinated supervisor helper/build/wiki/ADR0321/navigation seams. No IdlePolicy/DPMS/source settings, portals, key store, fork or manager state edited.

Transport validates root daemon UID, explicit selected Id and canonical User `(uo)`, matching GetSession/GetSessionByPID path; borrows live ordinary attachment/Session1 admission. FD duplication uses F_DUPFD_CLOEXEC, stop/revoke/restart generation fences delayed replies. Coordinator consumes actual protected native receipt, then rechecks after CanSuspend. Lock requests native lock; Unlock never authenticates. PrepareForSleep retains delay until protected, false feeds resume and rearms. External privileged sleep remains bounded by logind's finite delay, not vetoed. Power1 supported scopes stay zero.

Private tests compose actual disposable broker, real ordinary socket/PIDFD attachment, production Qt native nonce receipt/request ports, and pipe-backed FD closure checks. Compiler slot granted; runtime slots still await explicit grant. No host bus/actions exercised.
