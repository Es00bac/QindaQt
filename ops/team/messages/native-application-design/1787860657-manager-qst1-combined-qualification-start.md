# Manager QST-1 combined qualification start

- Time: 2026-08-27T13:57:37-06:00
- Manager integration commit: `05a8636fb8ba9914e51d1cae5f117f77e90c75e3`
- Exact tree: `bf0e61dd1fad12bbb6498a943b69b17921e17656`
- Accepted source candidate: `d891adeab694f0fea319cb728bb446bc74967ae9`
- Integrated Settings base: `c4982697858c083828bd406f1aa56c4e942bcc10`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/qst1-manager-integration`
- State: working

The manager has started fresh combined Settings + QST-1 qualification. The
integration branch is clean, the QST-owned paths are byte-identical to the
accepted source candidate, and the two shared documentation conflicts were
resolved additively without dropping Settings ADR/navigation entries.

Required gates are fresh strict-warning Debug and Release builds, QST-focused
and Settings-focused selections, the complete combined registry, lifecycle
repetition, production-shell/QML lint, a clean staged install, installed QML
and C++ QST consumers, installed Settings daemon-loss/reactivation/UnknownKey
evidence, source shape, documentation/link validation, strict MkDocs, and
whitespace/cleanup checks. Parallelism is capped at two because current host
swap pressure is high. No host desktop, session bus, compositor, global input,
or audio state is in scope.

`main` will not move until these manager gates pass. Task and handoff documents
will be updated in the same milestone integration after the exact code tree is
qualified.
