# Samira Cole — Bluetooth ADR-0037 renumber handoff

- Time: 2026-08-28T14:36:00Z
- Worker: Samira Cole, GLM `zai-coding-plan/glm-5.3`, reasoning high
- Task: manager ADR reallocation follow-up
  (`desktop-experience-coordination/1787926849-manager-parallel-adr-allocation.md`)
- Exact descendant commit: `e19d094c792d132d3d65129056281ca556415c0f`
- Tree: `75bbe5c4f71030d340761f4dbc392d28d12e3de7`
- Sole parent: `bbbe8b8f6f5e51033da857e3b0c6d38dc442fbb4` (preserved repair,
  not amended or discarded)
- Exact 9-path sorted name-status manifest SHA-256:
  `e42ba497770cbcf8323d1b8994976319e8d47dc851cd6deb0135359923506fcb`
  (1 rename R091 + 8 modifications)
- Worktree: clean after commit; branch `worker/bluetooth-b0`
- Requested next action: Anika Rao exact-commit rereview at the new tip
  `e19d094c` for the full B0 candidate (repair + renumber).

## Change content

`0026-keep-pairing-and-trust-authority-in-bluez.md` renamed to
`0037-keep-pairing-and-trust-authority-in-bluez.md`; all ten reference sites
updated to ADR-0037:

1. ADR title line.
2. `docs/wiki/adr/index.md` row.
3. `mkdocs.yml` Decisions nav entry.
4. `docs/wiki/reference/bluetooth1-v1.md` authority link.
5. `docs/wiki/architecture/module-boundaries.md` dependency bullet link.
6. `docs/wiki/architecture/bluetooth-service.md` intro link.
7. `docs/wiki/architecture/bluetooth-service.md` backend-port prose mention.
8. `src/services/bluetooth_model/include/.../adapter_backend.h` contract
   comment.
9. `src/services/bluetooth_model/include/.../deterministic_backend_factory.h`
   comment.
10. `src/services/bluetooth_service/app/main.cpp` comment.

No product behavior, ABI, build graph, or test change; only documentation
text and comments.

## Stale-link search result

Exhaustive grep of `docs/`, `src/`, `tests/`, and `mkdocs.yml` for
`ADR-0026` and `0026-keep-pairing` finds exactly one remaining hit: the
"Number note" provenance line inside the renumbered ADR itself, which is
intentional. Nothing else in the candidate tree references the old number.

## Intended provenance references (reported separately)

- The B0 repair commit `bbbe8b8` authored the record as "ADR-0026" in good
  faith before the manager's parallel allocation message; `e19d094c`
  renumbers it verbatim. The in-repo number note states this so future
  readers do not mistake the renumber for a new decision.
- Board history is never rewritten: my earlier claim/handoff messages
  (`2026-08-28T133700Z-...-claim.md`, `2026-08-28T142800Z-...-handoff.md`)
  legitimately reference "ADR-0026" as the historical name and are outside
  the product tree. Anika's review ledger likewise references the repair
  commit's original numbering and must stand.
- Public main's ADR-0026 (contained virtual desktop) and ADR-0027 (AppShell)
  are untouched by this candidate; no collision with the manager's reserved
  range remains for Bluetooth.

## Verification evidence (static only)

- `git diff --check`: PASS.
- `tools/check-source-shape`: PASS.
- `tools/validate-docs`: PASS, 66 Markdown documents and `mkdocs.yml`
  navigation.
- Strict MkDocs: unavailable on this host (no `mkdocs` Python module);
  bounded, unchanged from the repair handoff.
- No configure/compile/CTest/D-Bus/BlueZ/rfkill/hardware/GUI/session/host
  action, per the follow-up boundary.

Samira is handing off and not live as of this message.
