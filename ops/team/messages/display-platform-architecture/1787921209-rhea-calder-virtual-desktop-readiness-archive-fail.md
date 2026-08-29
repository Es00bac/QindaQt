# Rhea Calder — virtual desktop readiness/archive material FAIL

- Timestamp: 2026-08-28T12:46:49Z
- Exact HEAD: `3320afdb4afad1c396b85add576f60d59e1d3b57`
- Fresh run ID: `ea96a7ab461ac31584da1174853368f7`
- Exact live result: package fixture PASS; boot FAIL after 15.36 s

The private identity repair is effective. The contained desktop starts its
compositor, private bus, services, session graph, Settings, Text Editor, and
probe, then performs **52** complete public-topology probe attempts across the
bounded 15-second readiness interval. The terminal error is an unhandled
`subprocess.TimeoutExpired` while waiting only 0.072 seconds for the previous
probe to exit.

The authenticated archive contains command/result metadata and all **60** logs
created in the run. However, each probe's stdout snapshot is consumed by the
readiness loop and not copied into its log; only stderr files are preserved.
Consequently the exact last pending topology reason is lost and the generic
deadline-edge exception masks the underlying evidence gap. This is a
deterministic-failure-reporting and failure-artifact defect, not a relaxation
request.

The compositor/app logs contain fontconfig warnings and the private bus records
contained portal activation failures due to the intentionally absent host
display/system bus/FUSE device. No host endpoint was reachable. The private run
root is absent and no owned process survives.

Rhea will extract the cohesive readiness/probe code from the threshold runtime
module, archive every consumed probe snapshot into its per-attempt log, and
translate a prior-probe deadline edge into the exact retained last-pending
reason. Focused units will pin both behaviors without changing topology
criteria or the 15-second bound. The same exact live row will then either pass
or durably name the remaining product evidence gap.
