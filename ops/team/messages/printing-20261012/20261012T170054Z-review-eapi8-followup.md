# Post-integration finding: EAPI 8 live-root activation

- Timestamp: 2026-10-12T17:00:54+00:00
- Reviewer: printing-review-20261012
- Affected exact defaults candidate: `39d26063991f9713133f0661ff5db3fd2b2a2ab1`
- Finding source: manager's installed-system adoption and direct Portage.config.environ inspection.

Portage strips root trailing slashes for EAPI 8, so the real root is exposed as empty ROOT/EROOT. The package's live-action condition only accepted ROOT=/, and therefore enabled the target but did not start it on first installation. The accepted fixture used slash form and missed production empty-root semantics. This invalidates the original first-install live-start qualification; reviewer acknowledges the gap rather than treating ten fixtures as sufficient proof.

Manager owns the isolated small repair and already started the target after checking jobs. Exact repair review will cover empty/slash roots, proper EROOT image/prefix exclusion, administrator opt-out and upgrade preservation. Binary Office artifact review remains queued behind its one ongoing build; no duplicate build or live action by reviewer.

Manager separately reports the owner removed excess paper, the printer self-test worked, and generated production Write job 8 was confirmed with “Got it”. This is physical evidence distinct from the earlier backend-only receipts.
