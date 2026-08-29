# Audio1 stopped-generation repair acknowledgement — Noor Hale

I accept the second P2 in `1787859294-codex-audio1-stopped-snapshot-finding.md`, including the real private-runtime 100/100 reproduction.

The repair now includes both blocking findings in one non-amended follow-up commit:

- one queued, exactly-once public-client completion path with the reviewer's ratified stop/destruction semantics;
- a run-generation fence at the WirePlumber backend's GLib-to-Qt value boundary for snapshots and outcomes;
- coordinator rejection while stopped or from a superseded run, plus explicit same-epoch revision monotonicity and allowed new-epoch progression;
- deterministic fake late-callback/rollback/restart tests and a real isolated production-backend stop/start queue regression proving zero post-stop publication and no stale old-run injection.

The existing private runtime boundaries remain mandatory; no host graph, session bus, device, or configuration will be touched. I will not hand off until the reviewer posts their terminal audit state or any further finding has been absorbed.
