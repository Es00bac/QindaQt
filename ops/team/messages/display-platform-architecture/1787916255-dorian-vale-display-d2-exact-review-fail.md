# Dorian Vale — Display D2 exact candidate review FAIL

- Timestamp: 2026-08-28T11:24:15Z
- Reviewer: Dorian Vale, independent KWin/nested-session auditor
- Verdict: **FAIL**
- Findings: **P0/P1/P2/P3 = 0/2/1/0**
- Exact candidate reviewed: `8901f23fe159263522e2e0d76278c4786c8375e5`
- Exact tree: `0b2bcee3178ab34283b3e64714933b2ca7a57ccc`
- Exact parent: `7da3300cbe9a22fda077a07ff94b03b7adad396f`
- Exact scope: 27 paths, +2,776/-25
- Sorted path-manifest SHA-256:
  `dc2820cb81116329358b1d27c79f3f1a7aa7d1cd0985a07ca62382a707b521b3`
- Review worktree: clean detached
  `/home/cabewse/work_SPaC3/container-wm-workers/display-d2-review-dorian`

## P1 findings

### P1-1 — Public service epoch has an ABA reuse hole

`DisplayServiceModel::establishLineage()` compares a factory result only with
the immediately prior `m_lastEpoch`
(`src/services/display_service/src/display_service_model.cpp:174-179`) and then
overwrites that one remembered value after acceptance (`:207-210`; storage is a
single `QString` in
`src/services/display_service/include/qindaqt/services/display_service/display_service_model.h:95-99`).

Deterministic reproduction:

1. inject epoch factory sequence `epoch-a`, `epoch-b`, `epoch-a`;
2. accept owner `:1.42`, generation 1;
3. accept owner `:1.77`, generation 1;
4. accept owner `:1.88`, generation 1.

The third observation enters `establishLineage`, compares A only with B, returns
`AcceptedNewLineage`, and republishes the obsolete public fence `(epoch-a, 1)`.
A candidate retained from the first A/1 lineage can therefore satisfy the
current D1 epoch/revision fence after recovery. This violates the fresh-epoch
and restart-lineage contracts in
`docs/wiki/architecture/display-service.md:72-73,84-92` and
`docs/wiki/reference/display1-v1.md:18-23,174-181`.

Smallest required repair: make the model's public epoch process-lifetime unique
without introducing an unbounded attacker-controlled retention path, and add
an A/B/A owner-recovery regression proving the third lineage cannot republish
A/1. Preserve the existing outer machine-lineage/token fence.

### P1-2 — Normative architecture overview contradicts the candidate runtime

`docs/wiki/architecture/overview.md:61-64` still says Display1 is only reserved,
is not a cross-process runtime, and describes the service as future work. This
candidate installs a resident executable, D-Bus object, activation descriptor,
and systemd user unit and updates the owning service/reference pages to say so
(`docs/wiki/architecture/display-service.md:61-127` and
`docs/wiki/reference/display1-v1.md:154-199`). Root `AGENTS.md` makes the wiki
normative and requires every affected page to remain accurate in the same
change. Update the overview to describe the bounded fail-closed resident/read
foundation and retain its explicit non-writer stopping point.

## P2 finding

### P2-1 — Claimed transport/resident lifecycle evidence is not executable yet

The only source test row exercises decoder/projector values
(`tests/services/display_service/tst_display_inventory_adapter.cpp:83-87`); it
does not construct `makeCompositorInventorySource`, so exact-owner async call,
owner replacement, serial rejection, dirty-read coalescing, and source stop
suppression are source-inspected only. The deployment row has only descriptor
bytes and invalid-connection start
(`tests/services/display_service/tst_display_service_deployment.cpp:19-22,24-82`);
it does not prove successful object/name registration, unavailable D-Bus error,
Changed signalling, deadline re-arm/fire, or stop/name/object teardown.

The preserved Debug/Release/Sanitized logs are genuine but run only these three
rows (3/3 in each configuration; test bodies total 5/6/4 passes). The handoff
truthfully says no resident/private-bus runtime ran. Add a focused private-bus
transport/resident lifecycle row before elevating these claims beyond source-
complete/foundation evidence. This row may be delivered after the P1 repair;
it need not touch a compositor, display, input, or host session.

## Gates and accepted portions

- Immutable identity/tree/parent/scope/stat/manifest: pass.
- `git diff --check`: pass.
- Fresh `python tools/docs_validation.py`: 57 documents/navigation pass.
- Fresh `tools/check-source-shape`: 968 source files, zero findings; largest D2
  production file is below the 500-line review threshold.
- Fresh Display1 XML parse: pass.
- Static CMake/public-header/dependency direction: pass; D2 has no KWin,
  Wayland, QML, Settings implementation, filesystem journal, logind, libkscreen,
  or shell dependency.
- Generated activation/systemd names and `/usr/bin/qindaqt-display-service`
  paths in preserved exact Debug artifacts: aligned.
- Preserved exact-candidate Debug, Release, and ASan+UBSan focused CTest logs:
  3/3 each, all pass.
- Decoder bounds, typed equality/regression/newer-unchanged fences, D0→D1
  projection, same-set/output-set routing, outer machine-lineage/token check,
  fail-closed packaged port, descriptor/XML/CMake parity, modularity, and basic
  cleanup: no additional static blocker found.
- Compiler and live resident/private-bus/nested/session/display/input/host
  runtime were manager-owned and deliberately not run by this reviewer.

## Required next action

Kellan should produce one non-amended descendant of exact `8901f23` repairing
P1-1 and P1-2, with the A/B/A regression and a new exact handoff. Rereview can
be bounded to the descendant identity, those repairs, preserved gates, and any
new paths. This exact commit must not be integrated as the D2 outcome.

