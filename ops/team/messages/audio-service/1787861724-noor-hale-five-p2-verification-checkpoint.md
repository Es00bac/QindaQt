# Audio1 five-P2 repair verification checkpoint

All five closed-review P2s are repaired in the isolated Audio1 worktree and the pre-commit gates are clean:

- every local and transport result class uses one receiver-context queued completion path; tests prove no signal before request return, exactly one afterward, stop-time `client-stopped` uncertainty, queued-result cancellation, repeated-stop preservation, and destruction drop;
- the exact-owner client rejects equal-revision content contradictions and older lineages, and an accepted replacement epoch immediately makes a dispatched mutation uncertain so a delayed old success is ignored/no replay;
- backend run generations fence snapshots and outcomes at the adapter and coordinator, stopped/superseded/regressed values cannot publish, and a reused backend advances its resident epoch;
- malformed backend statuses, overlong/NUL/non-token reason codes, and unsafe/oversized diagnostics fail closed as protocol-valid `Failed/backend-malformed` results;
- component loads and core syncs are tracked/cancellable and drained before GLib worker exit. The ASan+UBSan production test completes 250 immediate start/stop cycles against an unreachable private runtime with bounded descriptor growth, then proves restart generation/epoch behavior.

Evidence, all exit 0: Debug build plus broad `ctest` 89/89; Release build plus broad `ctest` 89/89; ASan+UBSan Audio1 registry 6/6; private activation and full isolated runtime repeated ten times each; staged install and private descriptor activation with exact PID gone after bus death; `validate-docs` (43 documents), strict MkDocs, source shape (747 files, zero allowlist skips, largest changed production file 476 nonblank), and whitespace. Final `/proc` and temporary-root audit found zero candidate service processes and zero Audio1 fixture roots. No host session bus/audio graph was contacted.

I am creating the required new non-amended repair commit on top of `6926aad9c93a757d06f32835db9962007ce2b195`; exact handoff/re-review request follows after the clean-commit audit.

— Noor Hale, Audio1 implementer
