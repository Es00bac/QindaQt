# Backend partial-result repair exact handoff

- Exact tested runtime/source: f9c6e76ee35c12c355fa75bd4edf0b059cd20803, descendant of frozen42b4ebc1ffffcfbc0801ae501bf25ddc83f1b8eb.
- Same isolated worktree/branch, explicit qinda local hub preserved. Public API unchanged.
- Repair paths: src/services/agent_usage/src/agent_usage_collector.cpp, tests/services/agent_usage/{fixture.py,tst_agent_usage.cpp}; own worker/reply records.
- Reviewer independently reproduced original42 generic detail (7pass/1fail probe), so42 was not accepted. Repair now tracks both RPC success flags and publishes Ready plus fixed explicit Partial detail if only quota or only token metadata succeeds. Valid fields survive; unavailable fields stay absent. Raw RPC errors never appear publicly.
- Added data-driven quotas-only/totals-only assertions of exact partial detail and metric presence/absence.

## Exact direct verification

After reviewer explicitly released compiler, ran on exactf9:

`cmake --build .cache/agent-usage-build --target qindaqt_agent_usage_tests qindaqt_agent_usage_metadata_probe qindaqt-agent-usage-report -- $(portageq envvar MAKEOPTS)`: exit0, Debug strict, unchanged configured -j24 -l24; rebuilt owning collector and tests plus relinked publisher/probe.

`ctest --test-dir .cache/agent-usage-build -R '^qindaqt.agent-usage($|-publisher$)' --output-on-failure`: exit0,2/2 PASS10.31s. Observed Qt18/Python3, no failure/skip. Two partial data rows replace the former one, hence18 rather than19. Logs .cache/agent-usage-build/{build-review-repair.log,test-review-repair.log,Testing/Temporary/LastTest.log} (CTest temporary log below build root).

Prior strict MkDocs/docs518 and real sanitized Codex Ready/1quota/1token/0cost proof remain unchanged in scope; repair changes no wiki/nav or successful live read behavior. Reviewer separately reports original42 publisher27/27 adversarial assertions pass; final repaired combined recheck remains pending. Global source-shape baseline failures remain the previously bounded unrelated caveat.

Compiler released immediately to UI for exact merged combined230039dab1e3db490c67b5e2b19a4251fc3b22b7 native consumer8/8 and DPI gates, followed by same independent reviewer. No further source changes planned, no successor claimed. No install/publication, settings/credentials/transcripts, r15/Origin or active service mutation. Next action is exact final combined independent acceptance and manager integrated gates.
