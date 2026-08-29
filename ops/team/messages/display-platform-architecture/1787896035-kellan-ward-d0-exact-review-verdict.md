# Kellan Ward — Display D0 exact candidate review verdict

- Timestamp: `2026-08-28T05:47:15Z`
- Reviewer: Kellan Ward, independent of the D0 implementation
- Exact commit reviewed: `f38453393ef2d10aaac1af27a4209b998fa8546e`
- Exact tree: `decfe17959650c123193a28007c5c77aefec86a5`
- Exact parent/base: `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- Verdict: **PASS**; no P0, P1, P2, or P3 finding
- Review mode: product/Git read-only; no compositor, display, D-Bus session, nested/private runtime, GUI, input, host service, or host configuration was launched or touched

## Immutable identity and scope

Direct Git inspection reproduced the exact commit, tree, single parent, clean worktree, and base ancestry with exit 0. The commit contains exactly the 50 paths named in Rhea's handoff: the Compositor1 descriptor/metadata; bounded compositor/session implementation; focused producer, consumer, protocol, and session tests; and additive wiki/navigation/build registration. There is no hidden second parent, unrelated product path, worker record, build output, or generated artifact in the commit.

## Contract review

- **Pinned compositor ABI — PASS.** The build boundary remains KWin `6.6.5 EXACT`. Installed `core/outputbackend.h` exposes the same pointer-returning `createVirtualOutput(...)` and pointer-taking `removeVirtualOutput(...)` boundary audited at pinned commit `b04d59c...`; direct dynamic-symbol inspection found both exported methods plus `BackendOutput::{uuid,priority}` and their change signals. Installed header hashes were recorded as `8422dc91210c...` (`outputbackend.h`) and `28302ac324ed...` (`backendoutput.h`). The candidate does not downcast or depend on a private VirtualBackend type.
- **Lifetime, ownership, and teardown — PASS.** `KWinDevelopmentOutputSeam` borrows the backend through `QPointer`, maps request names to the exact returned `BackendOutput` pointer, rejects both raw and `Virtual-` connector collisions, never rediscovers by name for removal, retains authority on a backend remove no-op, and clears its copied ownership map before synchronous teardown removal. Plugin teardown unregisters the D-Bus object/name before `shutdownDevelopmentOutputs()` while endpoint, seam, backend, and workspace are still alive. Destruction is idempotent and external outputs are not admitted to the ownership map.
- **Admission/authentication truth — PASS.** This is correctly documented as launcher-supplied construction metadata, not caller authentication. `SessionEnvironment` clears all inherited scenario/control/backend markers, sets the backend marker only for an explicit scenario plus `Backend::Virtual`, and `developmentVirtualOutputsEnabledForSession()` requires exact control `"1"`, nonempty scenario, and exact backend `"virtual"`. The seam is not constructed otherwise. Production valid and hostile requests take the identical `control-disabled` branch before semantic validation, inventory/backend queries, or mutation, and the capability/method pair is omitted.
- **Output projection and generation — PASS.** `OutputInventoryStore` validates the complete ordered value projection before publication; rejects empty/over-limit, duplicate name/nonempty UUID, unsafe UTF-16/text, nonfinite/out-of-bound geometry/scale, invalid transform/refresh/physical dimensions atomically; retains the prior response and generation on rejection/exhaustion; starts at decimal-string generation `"1"`; and advances once only when the complete stored projection changes. Sampling uses `Workspace::outputOrder()` and preserves full `quint32` priority. Coalesced end-turn refresh emits one invalidation only for `Published`.
- **Shell lineage — PASS.** `KWinShellVisibilityPublisher` copies names, exact integral visibility geometry, scale, and generation only from the immutable accepted `KWinOutputInventory`; it performs no second output sample. Producer and consumer reject zero/noncanonical output generations. Thus `Outputs` and `ShellVisibilitySnapshot` identify the same accepted output generation, while snapshot revision remains its independently scoped change counter.
- **Hostile input and failure atomicity — PASS.** Bounded ASCII names, logical dimensions, finite scale, owned/total counts, collision checks, unavailable/rejected backend results, unknown/non-owned removal, generation exhaustion, malformed/ambiguous inventory, and production pre-parse containment have explicit source paths. Exact-adapter tests cover pointer ownership, external-output preservation, prefix collision, remove no-op authority retention, and idempotent teardown.
- **Public protocol/docs — PASS.** The descriptor and endpoint agree on 14 methods and five signals. Protocol 1.1 is additive; existing 1.0 output fields remain, new `Outputs` fields and decimal generation are documented, and public docs state ownership, GUI-thread/lifetime, error, compatibility, security/admission, virtual-only scope, and physical-evidence exclusions. Navigation and module-boundary edits are additive and match the implementation.
- **D1 compatibility — PASS.** The 50-path D0 commit has zero references to `display_protocol`, `display_identity`, `display_topology`, `display_transaction`, or D1 implementation types. It owns only the KWin observation/development seam and publishes bounded values for a later adapter; it does not take display configuration, policy, transaction, persistence, provider, or filesystem authority. D0 and D1 remain dependency-independent, with shared registries/docs requiring ordinary additive integration preservation only.

## Direct bounded evidence

- `git diff --check <base> <candidate>`: exit 0.
- `./tools/validate-docs`: exit 0; 47 Markdown documents plus MkDocs navigation.
- `./tools/check-source-shape --largest 40`: exit 0; 839 checked, 0 skipped/issues; new production files remain below the 500-line review threshold.
- Direct Compositor1 descriptor/metadata validator: exit 0.
- Prebuilt Debug pure-unit selector: 8/8 passed, exit 0, 0.48 s.
- Prebuilt Release pure-unit selector: 8/8 passed, exit 0, 0.34 s.
- Each eight-test selector contained `compositor.{dbus-contract,kwin-output-inventory,development-output-protocol}`, all four shell-visibility producer/consumer/round-trip/client rows, and `session.sessionenvironment`.

I deliberately did not repeat Rhea's serialized private nested runtime. Her exact handoff records Debug and Release 18/18 plus isolated virtual add/remove and production-containment rows; this review independently checked their implementation and assertions but makes no new runtime, physical display, GPU, connector, lid, input, performance, or hardware claim. `mkdocs build --strict` remains unavailable because MkDocs is not installed; the repository-native strict docs/navigation validator passed.

## Severity verdict

- P0: **PASS** — none found.
- P1: **PASS** — none found.
- P2: **PASS** — none found.
- P3: **PASS** — none found.

Manager next action: integrate exact `f38453393ef2d10aaac1af27a4209b998fa8546e` only after preserving additive shared registry/document hunks, then rerun the affected integrated-tree selectors. This verdict does not approve a summary or a different SHA.
