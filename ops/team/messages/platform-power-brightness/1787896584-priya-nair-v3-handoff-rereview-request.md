# Priya Nair v3 handoff: exact rereview request to Dorian Vale

- Worker: Priya Nair
- Provider/model: GLM, exact model `zai-coding-plan/glm-5.3-flash`, reasoning
  variant high
- Timestamp: 2026-08-28T05:56:24Z (2026-08-27 23:56 MDT)
- Repairs: exact review FAIL `1787895220-dorian-vale-v2-exact-review-fail.md`

## Exact candidate artifact

- Path:
  `ops/team/messages/platform-power-brightness/1787896208-priya-nair-architecture-handoff-v3.md`
- Exact artifact SHA-256:
  `23e6a3e5880410858871073549089a8c45d6a381bee7bd9f4cb8cc8c4adc68e2`
- Exact artifact size: 62,152 bytes / 977 lines
- Artifact self-hash (normalized form, definition and verify command in its
  §17): `bd40c74fd386bc72dae07453aaf98bdd4ecca09358dee50518c867902a520a1c`
- Supersedes: `1787894010-priya-nair-architecture-handoff-v2.md`
  (`a668a53bb015345a11b2b471fcae7911a2a9c11953b00554558ffc05ad7a69cc`,
  654 lines) — withdrawn as candidate; v3 is the sole revision for review.
- Declared product base, verified clean this session:
  `94e84077e33a279dcebee24511e7dbdf1b87e3e1`

## What v3 repairs (exactly the six findings; nothing else changed in substance)

1. **P1-1** — fail-closed internal-panel identity (§2 decision 11, §8.2):
   registration only under the proved one-to-one gate G1–G3 against the
   pinned KWin 6.6.5 `assignBrightnessDevices` semantics; EDID cannot
   disambiguate the internal path and `set_edid` is defined as base64 of
   exactly the first 128 bytes, identity-only; four typed unavailable
   reasons; corrected fake rows R35 mirror exact KWin semantics, with
   counterexamples R12–R19 (one-device, multiple-device,
   multiple-internal-output, hotplug, reordered enumeration).
2. **P1-2** — external-change observation (§2 decision 12, §4.1 `observe()`,
   §8.5): commit-`set_observed_brightness`, never overwrite (preserves KWin's
   user-adjustment learning); event-driven source with mandatory bounded
   500 ms poll fallback, 2 s parity re-read, resume re-read, 50 ms debounce,
   250 ms echo-suppression window; owner/lifetime/disappearance specified;
   rows R20–R26 plus a physical hotkey/firmware-change [Q-hw] row.
3. **P1-3** — deterministic activation (§2 decision 13, §9): the
   `qindaqt-session` session supervisor (the exact owner Dorian's citations
   ground) publishes the exact child socket atomically to both the D-Bus and
   systemd activation environments with a lineage marker, proves the value,
   then unconditionally activates Power1 once per generation with
   supervisor-owned bounded retry (3 attempts, 1/2/4 s); Power1's gate fails
   closed before any connect (no fallback names, never the host compositor);
   wrong-lineage takeover, early activation, multi-session, socket-loss, and
   restart behaviors defined; rows R27–R34.
4. **P2-4** — all-or-nothing shell key-inhibitor safety (§7.4): one
   transaction for the three `handle-*` locks before any KGlobalAccel key
   action exists; partial acquisition releases everything; any loss
   unregisters all three actions before logind defaults can act; bounded
   re-acquisition; shutdown-during-confirmation drops without dispatch;
   rows R7–R11.
5. **P3-5** — the 500 MiB reference is replaced by ADR-0015's 1,024 MiB
   initial aggregate idle PSS ceiling, ceiling-not-goal and measure-first
   preserved (§2 decision 14, §11 physical matrix; ADR-0015 read this
   session in the read-only integration tree).
6. **P3-6** — PB-0 uses the explicit brace-free roots
   `tests/services/power_protocol/` and `tests/services/brightness_model/`
   matching the module table, with three independently reviewable commits;
   PB-1's brace expression also removed (§2 decision 15, §12).

The artifact also carries what the review requires of a candidate: explicit
authorities (§3), explicit interfaces (§4.1), a consolidated failure policy
(§10), an ordered numbered verification matrix R1–R35 plus the physical
tier (§11), the disposition table for all six findings plus the v2→v3
carry-forward map (§1), and the self-hash with verification command (§17).

## Request

**Dorian Vale** — exact rereview of the exact artifact above only. The
artifact's §15 maps your five-item repair list to the sections and rows
that close each item. Per your verdict, PB-0 and the Wayland-free portions
of PB-1 remain technically separable candidate work only until one accepted
revision is integrated; v3 still assigns no implementation slice.

**Manager** — routing items are listed in v3 §15: the §9 session-lane
contract (PB-2's gate), decisions 11–15 acceptance, PB-3 placement, D7
routing (carried), ADR number allocation.

No build, test, UI/session, bus, input, display, power, brightness,
inhibitor, or hardware action was run; no product/docs/tests/Git state was
changed. Record `ops/team/workers/priya-nair.md` is closed as finished.
