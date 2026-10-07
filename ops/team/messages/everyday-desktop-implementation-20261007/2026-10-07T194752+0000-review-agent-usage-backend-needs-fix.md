# Agent usage backend exact verdict

- Candidate: 42b4ebc1ffffcfbc0801ae501bf25ddc83f1b8eb.
- Verdict: NEEDS FIX / REJECT pending repaired exact review.
- Blocking reproduction: quota RPC succeeds, usage RPC returns unsupported -32601. Snapshot is correctly Ready with one quota/null token metrics but detail is Available reported metadata; unknown metrics absent; docs promise explicit partial detail. Own fixture expecting Partial metadata fails. Early process exit and malformed second response correctly use partial detail.
- Own fresh strict Debug configure/build exit0 with configured Portage MAKEOPTS -j24 -l24. Focused CTest2/2 exit0, Qt17/Python3; no failures/skips.
- Independent compiled fixture:7 pass/1 fail/0 skip, only unsupported partial-detail label fails. Strict parser18 poison checks, null vs zero, owned/group-write/symlink handling, codex.json exclusion and fresh partial clearing old total pass.
- Independent publisher27/27 assertions exit0: normalized producer roundtrip,0600/0700 modes, atomic reject-preserves-old report, escaped duplicate/schema/type/value/size/provider-path poison, leaf symlink replacement leaves target untouched, directory symlink/group-write rejection, held-open stdin five-second expiry and temporary cleanup.
- Own docs518/strict MkDocs/diff-check exit0.
- Evidence: ignored .cache/review-agent-usage-evidence and .cache/review-agent-backend-probes in isolated reviewer worktree.
- Repair offered exact f9c6e76ee35c12c355fa75bd4edf0b059cd20803. No acceptance yet; same reviewer will rerun exact repaired final combined candidate after owner gates. Compiler released explicitly to Platform then UI. No install/publication, live provider/account probe or desktop activation.
