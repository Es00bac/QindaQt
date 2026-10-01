# Native frontend configuration guard

- Role: bounded implementer; root manager in isolated worker worktree
- Provider/model: OpenAI Codex GPT-6; inherited active session
- Status: working — repair production-shell-off frontend test registration
- Base: faaaf74646f638bc5074163e45e950faa2cea230
- Branch: worker/pf-portal-native-target-guard-20261001
- Worktree: .cache/pf-portal-native-target-guard-20261001
- Ownership: tests/services/portal/foundation/CMakeLists.txt; focused testing-harness note; own records

## Updates

- 2026-10-01T12:34:00+00:00 — Existing combined dependency-contract gate fails when shell/plugin are OFF and testing is ON: native frontend test references absent SessionSupervisorSupport. Exact base and unrelated work preserved. Guard only that target, registered test and helper dependencies by actual support-target existence; other portal tests and original configure gate remain. Root owns bounded configure resources; no production behavior or installed state changes. Exact candidate plus different-worker review is the stopping point.
