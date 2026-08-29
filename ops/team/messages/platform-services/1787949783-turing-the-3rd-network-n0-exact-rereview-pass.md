# Network N0 exact rereview: PASS (P0/P1/P2/P3 = 0/0/0/0)

- **Reviewer:** Turing the 3rd (OpenAI collaboration runtime; exact model and reasoning are unexposed)
- **Timestamp:** 2026-08-28T20:43:03Z
- **Exact accepted repair:** `c619acd34f051e715a1b3532e44fcfcfcce45116`
- **Tree / sole parent:** `4f85c643e00186e1b9a27892fc8126204d67d03a` / `e3e2719dfb3f76b119c4c6c7ccd1193012acff35`
- **Full series base:** `146fc48358c2659436dec4fc6b6062d23c5ee746`
- **Detached review worktree:** `/mnt/d/QindaQt/worktrees/network-n0-rereview-turing3`
- **Verdict:** **PASS**; manager may replay the exact two-commit Network series and run combined-tree verification

## Prior blocker closure

All eight independently reproduced rejection cases now pass in both the
registered hostile row and my reviewer-owned external executable:

1. decoded payload owner must equal the request/signal/current owner;
2. real A→B→A replacement retains epoch high-water and rejects retired A;
3. `INT64_MAX`/over-range lease duration and local-clock overflow fail atomically;
4. final diagnostic output, including ellipsis, stays within 512 UTF-8 bytes;
5. quoted, unquoted, suffix-shaped, and malformed credential fragments redact or fail closed;
6. U+202E and other unsafe presentation scalars are hidden/rejected while safe supplementary Unicode remains valid;
7. noncanonical booleans and `wireValid=false` values are rejected without changing the caller destination;
8. failed transport start clears live state and a second start calls the transport again.

The original P2 package/operations/docs defects are also closed. Final tree
relative to the full-series base contains no `ops/team/**` paths, machine-local
path additions, platform adapter, concrete transport, D-Bus, NetworkManager,
secret store, QML, or UI authority. ADR-0045 and the module/protocol/architecture/
testing pages state the three-module dependency, ownership, lifetime/threading,
error, compatibility, lease, lineage, secret, pseudonym, and frequency contracts.

## Independent executable evidence

- Fresh strict-warning Debug focused build: **64/64 steps**, exit 0, no warnings.
- Fresh strict-warning Release focused build: **64/64 steps**, exit 0, no warnings.
- Exact registered selector: **13/13 passed** in Debug and **13/13 passed** in Release, including hostile, isolated installed consumer, clean boundary, and policy poison.
- Direct QtTest totals: **118/118 passed** in Debug and **118/118 passed** in Release. The registered adversarial executable is **10/10** in each.
- The reviewer-owned external executable from the original rejection was rebuilt against the exact candidate libraries and headers: **10/10** in Debug and **10/10** in Release.
- Deliberate mutation sensitivity in a separate same-hash throwaway worktree:
  payload-owner, A→B→A high-water, lease cap, diagnostic byte cap, quoted
  secret, U+202E, `wireValid`, and failed-start rollback mutations each make
  their exact registered hostile case fail. Weakening QtDBus detection makes
  `qindaqt.network-boundary-poison` fail for accepting the injected poison.
  The mutation tree and immutable candidate tree were both restored/retained
  byte-clean at exact `c619acd` / `4f85c64`.
- Debug and Release staged installs contain only the three Network archives and
  fourteen public Network headers before building the isolated consumer; no
  unrelated whole-tree artifact is required. An out-of-build prefix request
  fails before creation/deletion with the exact confinement error.
- `tools/validate-docs`: **77** documents/navigation entries valid.
  MkDocs strict: pass. `tools/check-source-shape`: **1,175** files, zero
  violations. `git diff --check`, changed-source SPDX, full-series confinement,
  added machine-path scan, exact parent/tree/base, `git fsck --strict`, and
  final cleanliness: pass.

## Integration boundary

The accepted tree is the two-commit series `e3e2719` then `c619acd` from
`146fc483`; `c619acd` itself is exactly one non-amended child of the rejected
foundation. Current manager history has no Network leaf paths, but it has
advanced the shared documentation/nav and root `src/tests` registries. The
manager must replay both commits while preserving the additive union in:

- `docs/wiki/adr/index.md`;
- `docs/wiki/architecture/module-boundaries.md`;
- `docs/wiki/development/testing-harness.md`;
- `docs/wiki/index.md`;
- `mkdocs.yml`;
- `src/CMakeLists.txt`; and
- `tests/CMakeLists.txt`.

ADR-0045 is the manager-reserved Network decision number. After replay, rerun
the exact thirteen-row selector, strict docs/MkDocs, source shape, and combined-
tree build gates before advancing QQ-005 evidence.

## Remaining intended boundary

N0 is a pure reusable boundary, not end-to-end connectivity. Resident Network1
service ownership, a concrete transport, NetworkManager/secret-agent integration,
persistence, Settings/shell UI, physical radio mutation, and hardware
qualification remain N1+; this PASS makes no claim for those later outcomes.
