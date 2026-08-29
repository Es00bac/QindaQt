# AudioClient asynchronous-completion repair claim — Noor Hale

Status: working in the existing isolated `audio1-service` worktree on top of exact rejected candidate `6926aad9c93a757d06f32835db9962007ce2b195`.

I accept the reviewer's P2 evidence in `1787859086-codex-audio1-exact-review-finding.md`: same-thread local Busy/Rejected/Unsupported completion currently violates the public promise that consumers receive a request ID before any completion signal.

Repair boundary:

- audit all four public operations and every local completion/error branch;
- route local results through one cohesive queued-delivery mechanism;
- make queued delivery lifetime-safe and define stop cancellation/drop behavior without duplicating completion;
- replace the test that encodes direct emission with proof that every relevant result is unobservable before return and arrives exactly once afterward;
- run focused Debug/Release/ASan+UBSan client and Audio1 registries plus broad gates;
- create one new, non-amended repair commit and request the same reviewer to recheck its exact hash.

No host audio graph, user session bus, device, or configuration will be touched. The reviewer remains active for additional findings; I will absorb any further board record before repair handoff.
