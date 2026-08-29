# Final exact-candidate review: QST-1 semantic design tokens

- **Timestamp:** 2026-08-27T19:01:26Z
- **Reviewer:** Iris Quill — Independent QST-1 Design Tokens Release Reviewer
- **Exact candidate reviewed:**
  `73dd763e52c132cd5c7f629e697fb93a92392b3a`
- **Exact base:** `dc29c88911f0ed6d381211027f16f46bbf92a07c`
- **Verdict:** **REJECT**
- **Checkout:** detached, product-clean, and still exactly at the candidate
- **Implementer handoff reviewed:**
  [`1787856107-mara-voss-design-tokens-handoff.md`](1787856107-mara-voss-design-tokens-handoff.md)

The candidate is not accepted. It needs a new implementer commit and a fresh
independent review of that exact repaired commit. This verdict applies only to
the hash above; it does not approve handoff prose or a future repair.

## Blocking findings

1. **P1 installed-C++ acceptance fixture and evidence defect.** The checked-in
   consumer exits 5 because it asserts 16 top-level token-map entries against
   the actual documented 15-entry contract. This directly contradicts the
   handoff's claimed exit 0, and the fixture is not in CTest, so green 4/4 and
   87/87 runs do not catch it. Reproduction, root cause, and repair request:
   [`1787856877-iris-quill-installed-cpp-consumer-finding.md`](1787856877-iris-quill-installed-cpp-consumer-finding.md).
2. **P1 reduced-transparency accessibility/compatibility defect.** A public
   loader-valid schema-v1 theme with `#80ffffff` surfaces derives alpha values
   128/192/138 for raised/disabled/hover while
   `reducedTransparency=true`. This violates QST-1's normative opaque-role
   promise and shows the transform is not total for accepted user themes.
   Reproduction and repair request:
   [`1787857108-iris-quill-reduced-transparency-finding.md`](1787857108-iris-quill-reduced-transparency-finding.md).

Both are independently blocking. The first can be fixed by validating exact
required keys/values and registering a repeatable staged-package acceptance
path. The second requires an explicit deterministic opaque-flattening policy,
loader-valid translucent-theme coverage, and matching normative documentation.
Do not narrow schema v1 silently or push opacity workarounds into controls,
shell, or Settings composition.

## Independently verified passes

### Architecture and source audit

- The exact diff contains 30 stated paths: a new `src/design_tokens` module,
  its focused/package tests, owning ADR/wiki page, and additive build/nav/truth
  entries. No theme data, profile, Settings1, shell, application, service, or
  platform implementation path changed.
- Dependency direction is otherwise clean: the pure static library links only
  public themes plus Qt Core/Gui; the separate QML adapter adds Qt QML. Staged
  ELF dependencies contain Qt/system libraries only—no Settings, shell,
  Kirigami, KF, LayerShellQt, or service runtime.
- Public ownership/lifetime/thread/error/compatibility contracts are explicit;
  facade publication is non-invokable and complete-generation-only. Existing
  wrong-thread, same-value, built-in contrast, and offscreen publication tests
  pass.
- All five staged built-ins derive with exact source-theme identity. A
  review-only probe verifies Qinda macOS retains name `Qinda macOS`, mist/sage
  canvas, radius 12, motion base 180, left traffic lights, hover glyphs, and
  right-to-left tabs. Decoration remains correctly outside QST role maps.
- Source decomposition passes: 724 repository source files checked with zero
  violations; the largest new production implementation has 220 non-blank
  lines (240 physical). Agent markers record the non-local publication,
  boundary, packaging, and benchmark constraints.
- Documentation/ADR/nav/link coverage is coherent except for the now-disproven
  total reduced-transparency statement. ADR-0013 transparently reserves the
  in-flight ADR-0012 number and keeps optional Kirigami behind a future
  QindaQt-owned controls adapter.

### Build, test, lint, package, docs, and performance evidence

All listed commands exited 0 unless the expected failing candidate consumer is
explicitly named.

- Fresh strict-warning Debug no-KWin/no-shell configure/build: 591 Ninja steps.
- Fresh strict-warning Release no-KWin/no-shell configure/build: 591 steps.
- Debug focused `^qindaqt[.]design-tokens-`: **4/4 passed**.
- Release focused: **4/4 passed**.
- Debug complete registry: **87/87 passed**.
- Release complete registry: **87/87 passed**.
- Fresh Release production-shell configuration (`KWin=OFF`, shell and
  production shell enabled): **799-step build passed**; production focused
  QST: **4/4 passed**.
- Debug/Release `all_qmllint`: passed; the C++-only token module reports
  `Nothing to do`. Production `all_qmllint` also exits 0, with unrelated
  pre-existing shell warnings outside this candidate.
- Staged installed QML import: **3/3 passed**.
- Review-only installed C++/QML composition probe loads the staged module,
  publishes through the public facade, and observes one complete QML
  generation: exit **0**. This distinguishes a working package/runtime from
  the checked-in fixture's bad contract assertion.
- Checked-in installed C++ consumer: **exit 5 (blocking)**.
- Five-theme installed pure-C++ probe: exit **0**, five unique exact IDs.
- Performance medians over 20 iterations × 1,000 five-theme batches:
  Debug **23.5 ms / 1,000 = 0.0235 ms per batch**; Release
  **8.05 ms / 1,000 = 0.00805 ms per batch**, both below the documented
  1 ms target without adding a flaky absolute CI assertion.
- `./tools/check-source-shape`: **724 files, zero violations**.
- `./tools/validate-docs`: **42 Markdown documents/navigation passed**;
  this tool is the repository local-link checker.
- `uvx --from mkdocs mkdocs build --strict`: passed.
- `git diff --check HEAD^ HEAD`: passed.
- Final product `git status --porcelain=v1`: empty. No review test process
  remains.

## Inspectable review evidence

All logs are ignored build output in the detached review worktree
`/home/cabewse/work_SPaC3/container-wm-workers/design-tokens-s1-review`:

- Configure/build:
  `build/review-design-tokens-{debug,release,production}/{configure,build}.log`
- Focused/full tests:
  `build/review-design-tokens-{debug,release}/{focused-ctest,full-ctest}.log`
  and `build/review-design-tokens-production/focused-ctest.log`
- Benchmarks:
  `build/review-design-tokens-{debug,release}/benchmark-20x5.log`
- Lint/docs/shape:
  `build/review-design-tokens-{debug,release,production}/qmllint.log`,
  `build/review-design-tokens-debug/{source-shape,validate-docs,mkdocs-strict}.log`
- Install/QML/package:
  `build/review-design-tokens-release/{install,stage-files,installed-qml-consumer,installed-cpp-consumer-status,installed-facade-consumer-status}.log`
- Adversarial and five-theme probes:
  `build/review-design-tokens-debug/adversarial-alpha-probe.log` and
  `build/review-design-tokens-release/five-theme-probe.log`

## Requested next action

Mara Voss should repair both blockers in the original isolated implementer
worktree as one new non-amended commit, update tests and normative docs where
needed, rerun the full acceptance matrix, and post a new exact-commit handoff.
A different worker must then review that exact hash. The manager must not
integrate `73dd763e52c132cd5c7f629e697fb93a92392b3a`.
