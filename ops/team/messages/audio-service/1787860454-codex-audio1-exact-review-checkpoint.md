# Audio1 exact review checkpoint

Reviewer: Codex Audio1 exact reviewer  
Candidate: `6926aad9c93a757d06f32835db9962007ce2b195`

The exact candidate remains blocked by five posted P2 findings: synchronous
public completions, post-stop/stale-generation publication, client lineage
contradictions, malformed backend-result publication, and deterministic rapid
start/stop FD exhaustion.

Independent passing evidence so far:

- fresh Debug build `602/602`; Audio selection `6`, `6/6` pass; full registry
  `89`, `89/89` pass;
- fresh Release build `602/602`; Audio selection `6`, `6/6` pass; full registry
  `89`, `89/89` pass;
- Debug and Release activation + isolated WirePlumber tests each passed
  `--repeat until-fail:20` (40 executions per configuration);
- fresh ASan+UBSan Audio selection `6/6` pass;
- `validate-docs` passed 43 Markdown documents, `mkdocs build --strict`
  passed, source-shape passed 746 files, and base-to-candidate `diff --check`
  passed.

The reviewer-owned ASan lifecycle probe is intentionally failing as the fifth
finding (`FD_BEFORE=5`, `FD_AFTER=506` after 50 immediate start/stop cycles).
Production shell/QML build, staged install, actual installed-descriptor private
D-Bus lifecycle, and final exact-process/HEAD cleanup are still running. The
final decision will be an explicit exact-commit `REJECT`, with complete gate
evidence and caveats, after those remaining gates close.
