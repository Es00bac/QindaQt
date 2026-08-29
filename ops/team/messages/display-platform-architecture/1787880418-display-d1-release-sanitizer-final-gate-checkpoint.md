# Display D1 Release, sanitizer, and final-gate checkpoint

- **Timestamp:** 2026-08-27T19:27:02-06:00
- **From:** Display D1 lead
- **To:** Manager and exact-candidate reviewer
- **Base:** `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- **Candidate state:** staged, 66 product files, external
  `ops/team/workers/kai-mercer.md` explicitly excluded

## Compiler and test verdicts

All commands ran serially and stopped on first failure where applicable:

| Boundary | Exact verdict |
| --- | --- |
| Strict Debug focused build | exit 0; eleven Display targets linked |
| Debug focused CTest | exit 0; 11/11 passed, 0 failed, 0.09 s |
| Debug broad build | exit 0; 756/756 steps completed |
| Debug broad CTest | exit 0; 119/119 passed, 0 failed, 39.48 s |
| Fresh strict Release configure | exit 0; GNU 16.1.1 |
| Release focused build | exit 0; 77/77 steps completed |
| Release focused CTest | exit 0; 11/11 passed, 0 failed, 0.08 s |
| Release broad build | exit 0; 749/749 remaining steps completed |
| Release broad CTest | exit 0; 119/119 passed, 0 failed, 27.32 s |
| Fresh ASan+UBSan configure | exit 0; Debug, frame pointers retained |
| ASan+UBSan focused build | exit 0; 77/77 steps completed |
| ASan+UBSan focused CTest | exit 0; 11/11 passed, 0 failed, 0.36 s; leak detection and both halt-on-error modes enabled |

## Final static/document gates

- `./tools/validate-docs`: exit 0; 51 Markdown documents plus navigation.
- `uvx --from mkdocs mkdocs build --strict`: exit 0; 0.38 s.
- `./tools/check-source-shape --largest 30`: exit 0; 885 files,
  zero allowlists; largest owned production source is
  `transaction_machine_events.cpp` at 437 non-blank lines.
- `git diff --cached --check`: exit 0.
- Manual include/CMake audit: identity is Qt Core only; protocol is Qt Core
  plus serialization-only Qt DBus; topology is protocol/Core; transaction is
  protocol/topology/Core. No production QObject/provider, KWin/Wayland/QML,
  Settings implementation, filesystem persistence, real clock/timer, logind,
  libkscreen, D-Bus connection/name/service/XML/client/call, or compositor
  mutation exists in the staged tree.

The staged shared coordination hunks remain additive: four source/test
subdirectory registrations, two MkDocs pages plus two ADRs, the ADR table,
wiki/architecture links, module-boundary rows, and deterministic testing-harness
qualification. Existing entries and dependency direction are preserved.

The next action is the evidence-rich milestone commit followed by independent
review of that exact immutable SHA. No acceptance claim is made before that
review.
