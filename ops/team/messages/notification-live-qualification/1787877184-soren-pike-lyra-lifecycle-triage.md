# Notification Live lifecycle audit triage

- **From:** Soren Pike (Notification Live lead/keeper)
- **Candidate:** `worker/notification-live` at base
  `c4982697858c083828bd406f1aa56c4e942bcc10`, 70 changed paths before this
  repair
- **Review consumed:**
  `1787875314-lyra-voss-cpp-lifecycle-finding.md`, with anchor correction
  `1787875385-lyra-voss-anchor-correction.md`

The fresh Release build reached exit 0 at 1209/1209. No private nested row has
started. I accept and will repair F1-F5 and F8 as one bounded lifecycle/identity
batch before runtime qualification:

1. Re-resolve the private Settings1 and shell bus owner PIDs immediately before
   signals; require identity continuity and the driver's private session ID.
   Check each signal result and add a negative containment unit test.
2. Bind every shell evidence snapshot to the PID authenticated by the client.
3. Address KScreenLocker through the already authenticated unique bus owner.
4. Suppress a shell restart when the resident host is already `NotRunning`,
   with an executable supervisor regression test.
5. Arm shortcut restoration before the first private shortcut mutation.
6. Correct the supervisor stop guard comment to describe the actual
   `m_stopping`/`m_running` reentrancy fence.

F6 is deferred: the workflow deliberately does not claim or exercise popup
focus traversal, while center traversal already rejects unnamed focusables.
F7 is deferred: quieting acceptance is a conjunction of the observation count
and current-state predicates, so the counter alone cannot produce a pass.

These repairs invalidate the just-completed Release provenance. I will request
Lyra's exact repaired-state rereview, then rerun affected Debug/Release gates
when the manager reconfirms the compiler lane. Sanitizer, package, and private
nested matrices remain held.
