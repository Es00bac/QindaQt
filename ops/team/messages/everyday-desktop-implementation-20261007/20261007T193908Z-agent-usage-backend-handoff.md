# Agent usage backend exact handoff

- Frozen runtime/source candidate: 42b4ebc1ffffcfbc0801ae501bf25ddc83f1b8eb.
- Original corrected base: ccc99fb3f67c2bdc4f7156af7c0b0f1e05478057; inherited exact manager ADR/navigation a9746fb40131e6fc9f609412eeba87b5d0e48988 via merge dbac94be5272c6f28e8aa935e974fcc6a335969f.
- Worktree: /home/cabewse/work_SPaC3/container-wm.worktrees/everyday-agent-usage-backend-20261007.
- Branch: worker/everyday-agent-usage-backend-20261007; explicitly preserved to /home/cabewse/git/container-wm.git.
- Own paths: src/services/agent_usage/**, tests/services/agent_usage/**, docs/wiki/reference/agent-usage-reports.md; additive src/CMakeLists.txt, tests/CMakeLists.txt, mkdocs.yml. Intermediate tools/qindaqt-agent-usage-report Python publisher removed; final installed publisher is owning QtCore target qindaqt-agent-usage-report.
- Public QindaQt::AgentUsage headers/types/seam unchanged from UI agreement: owning snapshot(), explicit refresh(), snapshotChanged(), separate tokenScope/costScope, constructor-visible directory/program/clock and same-thread lifetime.

## Direct verification

Configured .cache/agent-usage-build with Ninja, Debug, BUILD_TESTING=ON, /usr, KDE_INSTALL_LIBEXECDIR=libexec, strict warnings ON, shell/production shell/KWin plugin OFF. Initial default-layout configure failed owning fixed portal contract before compiler; final accepted layout passes.

- cmake --build .cache/agent-usage-build --target qindaqt_agent_usage_tests qindaqt_agent_usage_metadata_probe qindaqt-agent-usage-report -- $(portageq envvar MAKEOPTS): exit0. Direct MAKEOPTS observed -j24 -l24; no overrides.
- ctest --test-dir .cache/agent-usage-build -R '^qindaqt.agent-usage($|-publisher$)' --output-on-failure: exit0, 2/2, 10.49s. Qt17 cases, Python3 tests, zero failure/skip. Parser bounds, duplicate keys, partial unsupported usage, single flight, stale/offline observation retention, cold errors, bounded process cleanup/output/deadline, capped noise/custom discovery, inode/symlink/hardlink/FIFO rejection, actual producer-consumer round trip and stdin expiry covered.
- Final actual installed /usr/bin/codex test-only metadata probe: exit0; sanitized output state=1 quotaWindows=1 tokenMetrics=1 costMetrics=0. Only state/counts retained; no raw account/payload/credentials/transcript output. Initialize/initialized followed by only account/rateLimits/read and account/usage/read.
- mkdocs build --strict --site-dir .cache/agent-usage-wiki: exit0, 8.02s; tools/validate-docs: exit0, 518 Markdown documents/navigation.
- git diff --check: exit0.
- Global tools/check-source-shape --warnings-as-errors: exit1 on existing unrelated large files; no owned agent_usage diagnostic. Preserved bounded caveat, no unrelated decomposition edits.

## Contract and next action

CLI Codex metadata overrides any codex.json; other provider reports use strict identity/source/scopes. Discovery inspects at most64 entries including noise, admits at most31 custom providers, caps snapshot40 and emits an explicit omission Error row. Descriptor-relative reads verify opened inode ownership/regular type/link count/size before reading. QtCore publisher has64KiB/five-second stdin bounds and private atomic0600 publication. Unknown metrics remain absent; Claude context tokens and session cost never combine into invented totals. Gateway spend percentages can exceed100 and do not become billed USD cost. Cold refresh visibly collects; subsequent collecting/wholly failed refresh retains last-known timestamped Stale values. Fresh partial responses never blend old totals.

Compiler lease released to UI. No installation/publication, provider settings/credentials/transcripts, daemon, inference, service/desktop state or immutable r15/Origin changes. Reviewed owning queue/thread after handoff; no successor task claimed. Next action: exact independent backend review and UI exact candidate merge/consumer gates, then manager integrated acceptance. Board-only follow-up maintains the stable Everyday Platform Sol record.
