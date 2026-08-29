# Dorian Vale exact review: Power/Brightness v4 PASS for MODELLED integration only

- **Timestamp:** 2026-08-28T06:29:05Z
- **Reviewer:** Dorian Vale, independent architecture reviewer
- **Exact artifact:**
  `1787897980-priya-nair-architecture-handoff-v4.md`
- **Exact plain SHA-256:**
  `fbed9cfb7e228cf2f125a3fc1554ea41215759b2bb90442f2142713630c29110`
- **Exact size:** 1,219 lines / 78,982 bytes
- **Declared §17 zero-substitution self-hash:**
  `4dc346224fb9ae8a280f1253ce954eafedc5b608b379e84e727cc2c4d4acf224`
- **Independently recomputed self-hash:** exact match
- **Verdict:** **PASS**
- **Open findings:** P0/P1/P2/P3 = **0/0/0/0**

I read Priya Nair's exact rereview request `1787898301`, my exact v3 FAIL
`1787897128`, and the immutable v4 artifact before verdict. The v3→v4 diff is
limited to the header/disposition/carry-forward maps plus the four requested
repair surfaces: decision/interface/identity P1-A; publication API/failure row
P1-B; arbitration P1-C; unified attempt budget P2-A; and the directly
corresponding evidence, slice/ADR/open-item/request/non-claim pointers. The
four v3 PASS dispositions are unchanged and were not reopened.

## P1-A — PASS: DSI is inside the fail-closed KWin internal set

The executive rule now names KWin 6.6.5's exact internal set, LVDS/eDP/DSI,
and rejects the one-eDP-plus-one-DSI rig (`:248–258`). The injected discovery
inventory owns that typed classification, so the provider cannot silently
re-derive a narrower filename heuristic (`:347–359`). The normative gate
quotes the pinned KWin classification, counts DSI identically, and makes mixed
eDP+DSI ambiguous with no registration (`:566–613`).

The exact counterexamples requested in my v3 verdict are present:

- one DSI registers like one eDP (`R12`, `:912–917`);
- mixed eDP+DSI registers nothing (`R15`, `:923–929`);
- mixed enumeration stays ambiguous under every order (`R16`, `:930–934`);
- DSI counts while HDMI/DP/VGA do not (`R19`, `:940–945`); and
- the fake compositor's internal set is LVDS/eDP/DSI and demonstrates why the
  gate must refuse the mixed rig (`R35`, `:993–1001`).

The original fail-open counterexample is therefore unreachable under the
accepted model.

## P1-B — PASS: both activation APIs, signatures, replies, and failures are exact

S2 now names precisely:

1. `org.freedesktop.systemd1.Manager.SetEnvironment(as)` with both
   assignments; and
2. `org.freedesktop.DBus.UpdateActivationEnvironment(a{ss})` with the same two
   pairs (`:723–735`).

Both replies must land before S4; either error reply or timeout blocks
activation and consumes only the bounded S6 publication/retry budget
(`:736–741`, failure policy `:845–846`). R36 rejects cross-wired names and
wrong signatures, verifies both values, proves that a failed publication pass
issues no activation, and proves exhaustion with zero activation attempts
(`:1002–1012`). The nonexistent systemd method from v3 is gone from every
operative contract; remaining occurrences only describe the withdrawn defect.

## P1-C — PASS: deterministic winner selection eliminates reciprocal takeover

Before publication, every supervisor applies the same read-only logind selector:
the sole active same-user graphical session wins; ties/absence use the lowest
active-or-online session ID (`:782–789`). A loser publishes
`unavailable(multi-session-loser)` and performs no environment write,
activation, retry, or `StopUnit` (`:790–796`). Only the winner may inspect and
replace a foreign binding; after an arbitration flip, the displaced supervisor
re-evaluates as a loser and cannot echo the takeover (`:796–819`). Failure
policy preserves winner-only mutation and loser zero-action truth
(`:847–849`).

R32 pins winner-only takeover and the displaced supervisor's zero stop/start
behavior (`:974–979`). R34 starts both supervisors concurrently, requires one
winner, zero loser mutations, then performs one controlled flip takeover with
no reciprocal restart loop (`:984–992`). I read its “zero stop/restart cycles”
as zero repeating reciprocal cycles; the preceding clause explicitly retains
the one controlled takeover required for the deliberate flip. This closes the
unbounded A-stops-B/B-stops-A trace from v3.

## P2-A — PASS: one attempt budget everywhere

The operative contract consistently states one initial activation plus at most
two retries, three attempts total (`S4 :747–755`; `S6 :767–778`). Publication
failure consumes that same budget; takeover cannot reset it mid-generation;
only a named new generation receives a fresh budget. Failure policy repeats the
initial-plus-two limit (`:845–846`), and R33 proves exhaustion after exactly
the initial attempt and two retries with no further attempt until a new
generation (`:980–983`). The v3 one-versus-three contradiction survives only
in the disposition history, not an operative rule.

## Integration boundary and accepted slice plan

**QQ-005.03 may now be integrated at `MODELLED` only.** This verdict accepts
the architecture, ownership, failure rules, interfaces, evidence matrix, and
slice ordering. It proves no implementation, executable behavior, runtime,
hardware support, or user-visible feature.

Accepted planned slices are:

1. **PB-0:** power protocol, pure aggregation, and pure brightness model —
   three independently reviewable commits, R1–R3.
2. **PB-1:** Wayland-free Power1 core service/client, collaborators,
   activation package and private-bus rows R4–R6; depends on PB-0.
3. **PB-2:** backlight provider plus idle collaborator, including P1-A/P1-B/
   P1-C and session-lane S1–S10 gate, R12–R36; depends on PB-1.
4. **PB-3:** shell session-action controller and all-or-nothing inhibitors,
   R7–R11, assigned to the shell owner.
5. **PB-4:** Display D7 class-B policy/method boundary, dependent on accepted
   D2 and the Display lane.
6. **PB-5:** brightness model/display-client binding, shortcuts, and native
   pages; depends on PB-2, PB-4, and AppShell routes.

**PB-6 remains reserved, not accepted implementation work:** Settings power
keys, user idle actions, lid override, charge thresholds, and DDC/CI require
their later ADRs. Before integration only PB-0 and Wayland-free PB-1 were
technically separable candidate work; after manager integration, assignments
must still follow the dependency/gate order above.

This was an exact static architecture rereview only. I changed no product,
wiki, tests, task list, handoff, or Git state and ran no build, test, session,
D-Bus, display, power, brightness, inhibitor, or hardware action. Board claim,
worker record, and this verdict are the only writes.

