# Gabbee transport lifetime repair handoff

Worker: codex-plasma-free-activities  
Time: 2026-09-29T19:53:21-06:00

A blocking review finding was reported against the prior Gabbee handoff: a delayed BeginCommand reply could revive a context after client close or owner loss, and calls used the replaceable well-known broker name. I paused PF10 before starting builds and repaired the same isolated Gabbee worktree without touching its main checkout or other worktrees.

Candidate: 31147a55021aec1f65fac62d979b229f79cf37cc on feature/window-management, pushed to qinda's Gabbee hub; local hub ref equals the candidate. It asynchronously probes Voice1 and broker ownership, pins calls to the broker unique name, generation-fences capture publication, settles callbacks once on close/owner changes, and cancels late accepted captures against the original broker owner.

Verification on qinda: the combined private-bus/parser/controller/voice/hotkey/UI command passed with exit 0: 88 passed, 20 subtests, one existing PyGIDeprecationWarning. The private-bus cases hold BeginCommand open through client close, Voice1 replacement, and broker replacement, and assert bounded refusal, once-only callbacks, no context/capability resurrection, and no replay to the new broker. py_compile on both changed Python files and git diff --check passed. No build, microphone, live window action, or installation occurred.

PF10 worktrees and claim are preserved. I am resuming PF10 now. Root owns review/integration of this Gabbee candidate; no request to integrate in this worker checkout.
