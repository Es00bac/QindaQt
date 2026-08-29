# Manager outcome: Display D0 compositor inventory and virtual-output seam

- **Timestamp:** 2026-08-28T01:13:04Z
- **Implementer persona:** Rhea Calder — OpenAI Codex `gpt-5.6-sol`, reasoning high; immutable role `Display D0 compositor-output lead`
- **Exact base:** `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- **Branch/worktree:** `worker/display-d0` at `/home/cabewse/work_SPaC3/container-wm-workers/display-d0`
- **Supervisor/integrator:** QindaQt manager; only the manager integrates
- **Authority:** manager decision `1787859005-manager-fable-display-decision.md`, Fable D0 handoff rows in `1787858968-elara-finch-fable-analysis-handoff.md`, exact KWin 6.6.5 pin and the current compositor/session wiki

## Whole user-visible outcome

The production Compositor1 output inventory has one explicit monotonic output generation and truthful stable fields needed by Display1 consumers. In a private development-gated nested QindaQt session, tests may add and remove a bounded virtual output through the pinned KWin backend and observe the same change in `Outputs` and `ShellVisibilitySnapshot`. The mutation API is unavailable and pre-parse rejected in ordinary production sessions.

## Owned boundary

Own the cohesive new output-inventory and development-output collaborators under `src/compositor/kwin/`, focused tests under `tests/compositor/`, and the smallest necessary integration edits to `qindaqtkwinplugin`, `kwincontrolendpoint`, compositor CMake, Compositor1 XML/service metadata/control codec, the compositor control reference, compositor-session wiki, testing harness, MkDocs navigation, and exact nested test fixtures. Preserve unrelated Hybrid behavior and do not refactor adjacent files.

Do not edit Display D1 service modules, Settings1, shell QML/panels, application UI, physical output configuration, KWin's persistent store, host Plasma services, or host session state. Do not read or write `kwinoutputconfig.json`. Do not run a compiler, nested compositor, or runtime test until the manager assigns that lane. Source/static work may proceed now.

## Required contracts

1. `Outputs` remains backward-compatible while adding bounded, deterministic `outputGeneration`, `uuid`, `priority`, `physicalSizeMm`, `manufacturer`, and `model` truth. Generation advances once per accepted changed inventory and never for an identical projection.
2. Output ordering and identity are deterministic; malformed/ambiguous/oversized inventories fail atomically. Existing shell visibility publication remains coherent with the same observed generation.
3. `AddVirtualOutputForTest(name,width,height,scale)` and `RemoveVirtualOutputForTest(name)` are bounded typed requests implemented through the exact pinned KWin 6.6.5 `OutputBackend::createVirtualOutput/removeVirtualOutput` public ABI. Validate names/dimensions/scale/count before mutation.
4. The existing pre-parse development gate is mandatory. With mutations disabled, malformed and valid requests both return `control-disabled` without parsing attacker-controlled payloads or touching the backend. Capabilities truthfully omit/disable the seam.
5. Failed add/remove operations leave the prior inventory and generation intact. Plugin teardown removes only D0-owned virtual outputs and publishes no stale success.
6. Public ownership, lifetime, GUI-thread affinity, error vocabulary, compatibility, and development-only security boundary are documented with concise `AGENT-CONTRACT`/`AGENT-GUARD` markers where non-local.

## Acceptance evidence

- Pure/unit tests for projection, generation stability/overflow, bounds, duplicate identity/name, atomic backend failure, idempotence/error cases, production pre-parse rejection, teardown, and existing control compatibility.
- Exact D-Bus contract/source policy tests and strict-warning focused Debug/Release builds/tests.
- Private nested add/remove evidence at 1080p proving `OutputsChanged`, new `Outputs`, and `ShellVisibilitySnapshot` converge to the same inventory/generation; production-disabled row proves no backend call for valid or hostile payloads.
- Broad Debug/Release registries, strict docs/link/MkDocs/source-shape/whitespace gates, process cleanup, and different-worker exact-commit review.
- Every report separates virtual nested proof from physical DRM/GPU/monitor evidence; D0 makes no physical hardware claim.

## Working rule

Own the complete outcome, not manager-invented fragments. Read the relevant board thread at claim, midpoint, material finding/help request, and handoff. Post exact file/line evidence. Preserve all useful work. Stop source-first at a coherent checkpoint if the compiler/runtime lane remains occupied; do not appear live while merely waiting.
