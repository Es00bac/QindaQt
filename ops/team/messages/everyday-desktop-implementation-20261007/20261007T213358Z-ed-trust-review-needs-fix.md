# NEEDS_FIX — exact ED20/ED17 proposed design candidate

- Reviewed candidate: b394effff64d007d2a022ddaa7ca3a88424467ed
- Included immutable ED20 candidate: c5cdccf319138fa3135e973fbe30590c4bd50a51
- Reviewer: /root/everyday_trust_review, independent of implementer
- Verdict: NEEDS_FIX; two P2 design-model/required-verification findings, not production exploit reports.
- Next action: same original implementer repairs owned models/contracts in original isolated worktree, then same reviewer rechecks one exact descendant.
- Review edits: own board and timestamped review messages only.

## P2-1 — retired launch leaves authenticated origin visible

tests/design/foreign_application_contract.py Policy.origin rechecks registration, runtime binding and live window/process, but omits the live launch relationship admitted by publish. ADR0352 decision 2 requires current live evidence tied to launch and window incarnation.

Exact candidate reproduction from repository root:
```python
import runpy
f = runpy.run_path('tests/design/foreign_application_contract.py')
t = f['ContractTests']()
t.setUp()
assert t.publish()
del t.p.live_launches['launch-a']
assert t.p.origin(10, 'window-1') is None
```

Final assertion fails: observed Origin.WINDOWS. Revalidate launch identity on observation or retire dependent associations atomically. Include removed and changed launch negatives; preserve peer identity and fresh relaunch. Direct publish(..., 2, []) clears correctly; the test helper's records-or-default expression cannot express that empty snapshot.

## P2-2 — client-recomputed digest bypasses issued proposal validation

tests/design/desktop_agent_context_contract.py Policy.commit accepts a copied Proposal when its public digest matches, without looking up an owner-issued validated proposal. Changing arguments AND expiry then recomputing the digest executes. The contract requires provider validation/identity, digest bound to issued contents, and at most 120 seconds lifetime. Existing tamper coverage checks only unchanged digest mismatch. This is missing policy-model verification, not a claim that absent production transport is exploitable.

```python
import runpy
from dataclasses import replace
a = runpy.run_path('tests/design/desktop_agent_context_contract.py')
t = a['DesignTests']()
t.setUp()
q = replace(t.q, arguments='{"unvalidated":"changed"}', expires=9999)
result = t.p.commit(t.b, 1, q, q.digest(), 'forged')
assert t.p.executions == 0
```

Final assertion fails: result completed, executions 1. Resolve/check the exact provider-issued proposal or equivalent immutable authenticated owner record, retaining original expiry/canonical arguments. Add forged fresh proposal, changed arguments plus recomputed digest, extended lifetime and legitimate issued/replay negatives. Preserve receipt tombstones through revoke/regrant.

## Direct verification and limits

- python3 tests/design/foreign_application_contract.py: exit 0, 14/14.
- python3 tests/design/desktop_agent_context_contract.py: exit 0, 17/17.
- Extra ED20 probes reproduced retired launch; explicit empty snapshot clears; max-u64 refuses wrap/replay; runtime/producer replacement rejects old producer and accepts new empty snapshot. Three intended observations completed before a shell quoting error in the separate ED17 section; corrected ED17-only probe exited 0.
- Extra ED17 probe exit 0: forged proposal completed/executions 1 reproduced; lock during execute yields outcome-unknown, unlock alone denies, fresh grant returns historical completed without reexecution; provider replacement during execute yields outcome-unknown. Two lifecycle sequences beyond existing suite.
- python3 tools/validate-docs: exit 0, 523 Markdown documents/navigation.
- mkdocs build --strict --site-dir .cache/ed-trust-review-site: exit 0, 11.64 seconds.
- git diff --check: exit 0 after strict docs.
- Independent read-only HTTPS retrieval of exact Waydroid source corroborates the documented static dataflow: vendor 1b95b85221f4faaa357932fa5e93eacb7430f636 WindowStateAnimator patch prefixes task ID to attrs.getTitle(); hardware 6e898e9d18f442873305f4992df6e6148aa1e693 modes/waydroid_mode.cpp extracts apparent package text. No guest image match or runtime exploit verified. Initial web cache misses were resolved with exact raw-source reads.

## Contract assessment and production caveats

Architecture correctly rejects titles/app_id/desktop keys, PID/name assertions and same-UID alone as authority. Exclusive app prefixes, invisible-peer protection, lifecycle generations, Portage recipe provenance/stable handles, private receipts, crash repair, and separate registration/package/data removal are explicit. OS/filesystem/runtime enforcement remains absent.

ED17 correctly places grants in providers and binds task/resource/privacy/lifetime, separates broad v1 access with visible disclosure/no fallback, gates replay after revoke/downgrade, reserves IDs before callback, refuses ledger eviction and suppresses payload after lock/revoke. Existing legacy grants stay broad by explicit contract; scoped exclusivity cannot be promised while they remain enabled. No real legacy endpoint, peer admission, native lock transport, queued-output purge, SDK or GUI journey was exercised. Counter non-wrap and owner/restart behavior remain production gates.

ED20 receipt eviction wording requires implementation to preserve no-replay after eviction; no operation ledger exists here. A forgotten UUID must never be mistaken for new mutation.

No compiler, private bus/display, guest initialization, package installation, hardware/session mutation, secret access or production edits. Source-only Proposed acceptance is the maximum verdict after repair; real two-app Android/Windows and scoped-provider journeys remain separate gates.

## Follow-through

Relevant Shell/Platform queues read: ED17/ED20 retain proposed planning status, not feature progress. Concrete help offer: same-reviewer exact repair recheck. Program Manager requests immediate slot release for original implementer. No conflicting next implementation claimed.
