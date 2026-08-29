# Sabine Cross — exact manager Notification integration PASS

- **Timestamp:** 2026-08-28T11:20:45Z
- **Reviewer:** Sabine Cross, Notification integration-boundary reviewer
- **Provider/model:** unexposed by this runtime; not inferred
- **Reasoning level:** unexposed by this runtime; not inferred
- **Exact manager commit:** `1b4e2846e40d31d79ffb03db2229c07ff9bca271`
- **Exact tree:** `c154d5cd29bbc6107ae049b9c7a84fda55a2d974`
- **Exact parent:** `b62e132e067842b51f95aeaa377efef1dfda9bc5`
- **Accepted source candidate:**
  `557260a50faaf083733afe5972ad6541ef398108` (tree
  `8f9f131461157b33bb88e0b4a46811e2308c9329`, parent
  `c4982697858c083828bd406f1aa56c4e942bcc10`)
- **Verdict:** **PASS**
- **Findings:** P0 0, P1 0, P2 0, P3 1

## Immutable integration proof

The source-candidate parent is an ancestor of the manager parent. I recomputed
both commits rather than accepting the handoff prose. Each commit changes the
same 74 sorted paths, and both path manifests hash to
`3be3d516f941c62d0d8f227258d0669fe71e336d787af9e7da3435755a98e731`.
The manager checkout is clean at the exact commit apart from its expected
one-commit branch lead, and `git diff-tree --check` passes.

A read-only synthetic three-way merge of manager parent `b62e132` with accepted
candidate `557260a` produced tree `4f20d852975932abaaad8abf528cd176097f844d`.
Within the 74-path candidate scope, 69 integrated blobs match that synthetic
result byte-for-byte. Git identifies exactly five content conflicts, not four:
`docs/wiki/adr/index.md`, `mkdocs.yml`,
`src/compositor/kwin/kwincontrolendpoint.h`,
`tests/compositor/test_dbus_contract.py`, and
`tests/session/compositorworkflow.cpp`. No conflict marker survives.

## Conflict-resolution and adjacent-contract audit

- The documentation registries preserve Notification ADR-0019/0020 and every
  pre-existing Controls/Text/Power ADR-0021 through ADR-0025 exactly once
  (`docs/wiki/adr/index.md:27-33`, `mkdocs.yml:80-86`). The docs validator
  confirms all 63 pages and navigation entries.
- The KWin endpoint header retains the Display development-output dependency,
  constructor port, shutdown hook, typed methods, and controller while adding
  Notification's exported layer-surface tracking and observation method
  (`src/compositor/kwin/kwincontrolendpoint.h:5-23`, `:44-53`, `:72-95`,
  `:114-118`). The implementation publishes both capability families and keeps
  production rejection ahead of Notification surface inspection
  (`src/compositor/kwin/kwincontrolendpoint.cpp:156-191`, `:224-288`,
  `:489-518`).
- The checked-in Compositor1 descriptor contains Notification surface evidence,
  Display virtual-output mutations, and compositor reinitialization with their
  distinct signatures (`compositor/dbus/org.qindaqt.Compositor1.xml:17-22`,
  `:46-63`). Its static contract preserves the complete method/event sets and
  validates both output-mutation and shell-surface signatures
  (`tests/compositor/test_dbus_contract.py:12-35`, `:96-163`). The exact
  descriptor/service validation passes.
- The session workflow retains the Display capability negotiation and dynamic
  Add/Remove method expectations beside Notification's
  `DevelopmentShellSurfaces` expectation (`tests/session/compositorworkflow.cpp:49-76`).
  Its production gate requires both the Display output-gate evidence and the
  Notification pre-inspection rejection, and records each accepted result
  (`tests/session/compositorworkflow.cpp:249-299`). The adjacent probe client
  retains both `developmentShellSurfaces()` and the D0/D1
  `outputsChangedCount()` observer (`tests/session/compositorprobeclient.h:63-86`,
  `tests/session/compositorprobeclient.cpp:190-209`).
- Candidate paths outside the five manual conflicts either remain
  candidate-identical or match Git's clean three-way merge exactly. The
  Notification host/shell/supervisor/harness slice is therefore not silently
  replaced by later Display, Controls, Text, or Power work, and those
  pre-existing contracts remain present rather than being reverted to the
  candidate's older base.

## Independent static evidence

- `tools/validate-docs`: PASS, 63 Markdown documents plus navigation.
- `tools/check-source-shape`: PASS, 980 source files, zero skipped files; the
  largest changed production seam is 485 nonblank lines.
- `python3 tests/compositor/test_dbus_contract.py ...`: PASS.
- `python3 tests/session/test_notification_live_unit.py`: PASS, 10/10.
- Immutable path-manifest comparison, synthetic-merge blob comparison,
  conflict-marker scan, semantic union assertions, and commit diff check: PASS.

I ran no compiler, CTest binary, nested runtime, GUI, host session, input,
display, audio, hardware, configuration, feature-score, TASK_LIST, or HANDOFF
operation. Runtime/build qualification remains the manager's separately owned
integrated-tree gate.

## Finding

**P3 — redundant reinitialization signature assertion.** The conflict
resolution retains the candidate's explicit extraction/check and then repeats
the same contract through `validate_method_signature`
(`tests/compositor/test_dbus_contract.py:133-152`). This is harmless and does
not weaken coverage or affect behavior, but a later cleanup can keep only the
shared helper call. It is non-blocking for this integration.

## Decision

Exact commit `1b4e2846e40d31d79ffb03db2229c07ff9bca271` preserves the independently
accepted Notification Live candidate and all examined pre-existing
Display D0/D1, Power, Text Editor, and Controls contracts. The manager may use
this PASS together with its separately owned integrated build/runtime results
to finish the QQ-004.05 qualification record.
