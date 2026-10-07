# ED-02/03 delivery prerequisite: Network installed QML fallback

- Accountable owner: Everyday Files Sol
- Exact base: f152d6c9ee04f99c01d4ca07c4dcb46701a42800
- Worktree: /home/cabewse/work_SPaC3/container-wm.worktrees/everyday-files-20261007
- Branch: worker/everyday-files-20261007
- Independent reviewer: Everyday Review Sol

Outcome: the installed Network QML module's disk fallback layout agrees with its qmldir and passes its actual component/package contract. Handoff states five nested fallback paths fail despite preferred compiled resources; reproduce that exact failure and fix the smallest owning install rule. This is needed to qualify the next desktop package; it is not an expansion of Network functionality.

Own src/apps/settings/network/CMakeLists.txt (verify actual owning module path first), focused owning Network package-contract tests and relevant network settings wiki. May request additive shared CMake/test registry changes from manager before editing. Own ops/team/workers/ed-files-sol-20261007.md and timestamped replies under this thread. Do not edit queues, task/handoff, features, overlay or another module's internals. Report path adjustment before any edit if initial owning path differs.

Acceptance: reproduce current disk fallback mismatch; installed component with preferred resources intentionally unavailable loads the five fallback paths; normal compiled module probe and focused Network/Settings tests remain green; poison/package isolation as owning harness requires; strict docs and link checker. Configure/build/test only on qinda in own source/build roots. Compiler allowed bounded -j4 unless taking the separate full-package lease; no private compositor/bus fixture without manager runtime lease. Never install or change active network/radios/services.

Read AGENTS, wiki index, releases, testing harness, network settings and applicable module contracts. Maintain exact live-board fields and ISO updates. Independent exact-commit review precedes integration. Handoff exact candidate, changed paths, commands/status/counts, evidence path and caveats. Preserve branch in qinda hub.

Next executable successor after accepted handoff: ED-04 design packet only: reproduce existing insertion discovery contract from source/disposable fixtures; propose public Removable Media observation/action boundary and File Manager/chooser consuming design in an ADR. That ADR must be reviewed before product implementation. Do not copy UDisks logic into File Manager. Ask manager for owned ADR number/paths when refilling this successor.
