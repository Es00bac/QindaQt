# Exact635 Windows child-authority refusal: read-only diagnosis

- Time: 2026-10-08T14:29:15+00:00
- Reviewed source: 635834b1c2e63a03c46b2646c164ef0ba4cafd25; manager boundary a5af147384a0fc94e89933c3aa8047a18d07cc41.
- Author: Platform. Reviewer: stable ed-foreign-astra-20261007, separate isolated review worktree. No source edits, imports of candidate code, child creation, signals, runtime, retry, namespace, compiler, Portage or host action.
- Outcome: actual FAILED result confirmed. Source identifies a polling/ancestry feasibility gap and inadequate complete-retirement proof; exact triggering child is unidentifiable from retained evidence. No native success or permission to rerun.

## Exact immutable evidence

Independently rehashed first-failure-proof.tar under manager .cache/windows-native-once-20261008:102400bytes, SHA256 f5f66de318e4dc535631fc61d8e61940dc2f13446c36c129890c6680ec21ec5e,18 regular members/17 indexed payloads; every indexed size/hash verified. Seven actual raw run files match archived bytes. Read full driver/outer/root process evidence and logs. Run is author .cache/windows-proof-runs/private-acrakhxj. All11 source hashes in outer evidence match actual .cache/windows-proof-source-v10-repaired files AND exact635 Git objects; no speculative source reconstruction.

Actual manager PID2390010/start48027884 exited1/4.306s, held wait returned/proc absent. Wrapper2390012/start48027891 exited1; namespace containment was recorded true. Those are wrapper facts, not proof of application-child settlement. Driver evidence SHA10cf232cb19e1c06588cb9802853ba630ffc82b858a5d2eb5c8debcbf74387ec has empty steps and no clients/two-window result. Initial native Windows inventory was schema2/ok/empty; XRes1.2 worked.

Two initial servers were admitted and separately retired: namespacePID21/start48028130 for app-a and23/start48028130 for app-b, same admitted socket peer PID/UID1000, distinct held prefix/server-directory/lock/socket identities. Each recorded exact pidfd SIGINT, exit0, dead/reaped, held lock release, current lock same and no replacement owner. Held app launch identities were PID22/start48028130 and26/start48028131. Neither app has a qualified retirement; uncertainCleanup contains app-a app, app-b app and child-ledger. This is not contradicted by server success. The app-b working-directory warning is not causal ancestry evidence, nor is the machine-id warning in server logs.

## What actually caused the refusal, and what is missing

Frozen child_ledger.py31–63 traverses /proc task children. For a live pid first observed with parent outside registered direct/previously observed sets,49–50 appends that numeric PID to a local unknown list;62–63 latches failure and raises the exact retained RuntimeError. This branch explains the actual error. It does not serialize unknown identities or the census. Driver catches only type/fixed message. The local unknown list, triggering PID/PPID/start/state, preceding ancestry and per-app association are absent from the preserved run and cannot be reconstructed after the namespace retired. Do not claim a particular Wine service, replacement server or app was the triggering child.

App cleanup further discards useful failure data: driver198–207 catches any app wait/nonzero/deadline/death error and records only a label, discarding actual exit and contain() result. Therefore its exact app exit cause is also unavailable. This is evidence incompleteness, not permission to reinterpret either cleanup as successful.

## Concrete source compatibility gap

The exact installed Wine source archive /var/cache/distfiles/proton-wine-11.0-2.tar.gz is56570548bytes/SHA c02269323d484befe3bc7628f13b445d6e1c6f44df9c4b97bc96bd4f96101f3a. Member dlls/ntdll/unix/process.c SHA4eb0a0b73aec3fbac9d68e0be8a6a4dc2fd83b1f8d6ae39e498f1ebbea10efac has real double-fork/optional setsid/exec paths at420–449,485–495 and589–618; the intermediate exits and its parent reaps it. This can reparent a Wine descendant to the checked driver subreaper before any Python checkpoint. The Wine source supplies a credible concrete source mechanism, not proof that a particular path ran in this attempt.

The existing test_child_ledger.py UNKNOWN_CHILD deliberately double-forks and exits before checkpoint, and its actual synthetic acceptance expects unknownRefused/noSignal. The KNOWN_CHILD positive artificially holds the parent until the ledger observes its child. Thus the tests prove the existing refusal policy, not its feasibility for unmodified Wine's unsynchronized process creation. Faster polling, another sleep, accepting executable/prefix/UID matches or treating every current driver child as one application's descendant cannot repair that authority gap. Driver141 also discovers window candidates by current ancestry from the initial app PID; a legitimate orphan can disappear from that mapping even if the cleanup refusal is removed.

## Separate complete-retirement boundary

Installed primary Linux man-pages6.17 were read directly. PR_SET_CHILD_SUBREAPER.2const.bz2 SHA9e3646b1a400be9ca570a353767d9f861837854f616ab27b5c41a4e14f93dcf2 specifies nearest living ancestor subreaper adoption and wait authority. proc_tid_children.5.bz2 SHA62eb2c03eb175d35f7e392ae739c5452d1ebae32107e9528c133cb7a9f680429 explicitly warns exiting children can cause live children to be omitted and reliable enumeration requires stopped/frozen children.

Frozen children() reads only /proc/pid/task/pid/children (the main task, not every thread). final()75–81 uses that live list both to reap and to declare no remaining driver children. It is therefore not a sufficient complete-retirement proof, even apart from the failed attempt. This review found no actual false-success run; all observed native evidence correctly failed. A repaired normal retirement needs kernel child-wait state, after all direct Popen owners have settled, until ECHILD. A nonblocking wait returning0 means children remain; it must never mean empty. Preserve wait status/lifetime and bounded deadline, no competing waiters. /proc snapshots can supplement diagnostics, not replace the kernel terminal condition.

## Smallest coherent repair proposal, subject to root ownership approval

Keep the current unknown-child refusal and never give signal authority to a PID merely observed by census, UID, prefix, cmdline, title, WM_CLASS or namespace membership. Add bounded failure observations first: checkpoint ordinal, supervisor identity, direct held identities, census PID/PPID/start/state, known/unknown classification and fixed refusal enum; include zombies in observation and retain actual app wait/containment outcomes. No raw environment/argv/auth cookies or global process dump. Observation failures remain refusals.

For two unmodified Wine programs, split lifecycle into two minimal per-prefix supervisor processes, each checked as a subreaper BEFORE it creates its fixed server and fixed app. Each starts childless, launches only its two declared Popen owners through one explicit constructor boundary, and remains alive through their retirement and all kernel-adopted child reaping. This creates a causal per-prefix ancestry domain; the current shared driver cannot recover that attribution after reparenting. It is a trusted-fixture boundary, not sandboxing arbitrary guest code or authenticated production origin.

Expose only a bounded private control/result interface on inherited close-on-exec owner-created FDs, with exact held supervisor lifetime, per-prefix generation/request nonce, current pinned prefix/server incarnation, sequencing and deadline. The supervisor owns the exact initial server Popen/pidfd and the existing lock/socket/peer guard; no server replacement admission. It supplies observed candidate process lifetimes within its domain for the driver's independent XRes/normal-window/fixed-PE checks. Main driver keeps the two domains disjoint; it does not infer per-app ownership from environment or a newly discovered process.

No adopted child needs signal permission for the normal success path: close windows, retire the same admitted server once, settle its direct application owner, reap adopted descendants until kernel ECHILD. Any live survivor, unknown/out-of-contract spawn, supervisor loss, incomplete evidence, nonzero abnormal app outcome, lost pidfd or replacement remains failure. On failure, only explicitly held owner processes retain their existing containment authority; outer namespace retirement remains separate and cannot fabricate success. The kernel guarantees ancestry/waitability, not a blanket right to signal every adopted child. Any desired expansion beyond this needs a separately reviewed authority contract.

## Executable controls required before another native grant

1. Real synthetic immediate double-fork/setsid with intermediate exit BEFORE the first census: correct isolated domain observes/reaps it without an intermediate hold handshake or adopted-child signal.
2. Two simultaneous domains with identical executable/UID/arguments: observations never cross domains; closing/retiring one leaves the other current. A driver/sibling peer is never admitted/signaled from its metadata.
3. Non-main-thread fork and a /proc-children omission control: kernel wait returning0 refuses complete retirement; ECHILD after direct-owner settlement is required. Zombie and naturally exited orphan status remains observable/reaped.
4. Supervisor dies/replaced, control EOF/replayed nonce, delayed receipt, pidfd/start mismatch, server loader replacement, prefix/lock/socket replacement: terminal refusal, no rebind/replay/foreign signal.
5. Live orphan after server stop, child flood, fd acquisition/read failure, output/write failure and deadline during final wait/publication: bounded failure with truthful retained evidence. Synthetic generations self-expire independently on all error paths; no new unbounded test children.
6. Existing server orderly SIGINT/lock/peer controls, no -k1/-w0 shortcut, final complete two-window/resize/close evidence, separate app exits and no-unknown-signals assertions stay mandatory. Exact old635 run remains failed. Native rerun requires fresh source/bundle/preflight review and a separate manager grant after release priorities.

Read-only review tooling notes: one rg included a nonexistent test_retirement.py and stopped before the archive read; corrected separately. One first man-page read omitted bz2 decompression and failed Unicode decoding; corrected read used bz2 and recorded actual hashes. Neither executed the candidate or changed failed evidence.

Requested action: root route the narrow lifecycle/diagnostic source packet to the same Platform author after R20 packaging priority, with exact path ownership and same-pair review. Reviewer is available for that exact source review; no resource held and no whole ED20/Windows qualification claimed.
