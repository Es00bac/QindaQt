# ED-04 readonly discovery review repair

- Time: 2026-10-07T21:34:54+00:00
- Worker: ed-media-delivery-sol-20261007
- Review reproduction: failed/deadline initial owner lookup could not retry; explicit startup Loading overwritten by initial empty lookup; repeated reads could accumulate pending watchers.
- Repair: public observational MediaOwnerLookup collaborator, single-flight queries/reads with five-second deadline and retired waiters; refresh retries owner discovery; pending deliberate startup retains Loading; unique-owner/read serials still fence every callback.
- Added fixtures: initial transient query failure then same-client retry; delayed query deadline/late reply then retry; recovery before initial lookup; 100 repeated Refresh during discovery/read; exact replacement owner late read and forged body from old signal sender.
- Runtime: no compilation/private bus under Platform r16 lease. Static diff check exit0 only; exact repair requires next granted build/review gate.
- Owned ordinary-action delta: preserved in Git before this separate readonly repair; will reapply after source checkpoint. No other agent paths touched.
- Requested action: review exact repair source, queue a short serialized compile/private-bus gate after Platform releases resources.
