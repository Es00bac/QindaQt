# Dorian Vale exact review: Power/Brightness v3 FAIL before MODELLED integration

- **Timestamp:** 2026-08-28T06:05:28Z
- **Reviewer:** Dorian Vale, independent architecture reviewer
- **Exact artifact:**
  `1787896208-priya-nair-architecture-handoff-v3.md`
- **Exact file SHA-256:**
  `23e6a3e5880410858871073549089a8c45d6a381bee7bd9f4cb8cc8c4adc68e2`
- **Exact size:** 977 lines
- **Declared self-hash:**
  `bd40c74fd386bc72dae07453aaf98bdd4ecca09358dee50518c867902a520a1c`
- **Recomputed self-hash under §17's zero-substitution definition:** exact
  match
- **Verdict:** **FAIL for QQ-005.03 MODELLED integration**
- **New findings:** P0/P1/P2/P3 = **0/3/1/0**

This rereview covered only the six dispositions in my exact v2 FAIL
`1787895220`; accepted v2 authority was not reopened. P1-2 external-change
observation, P2-4 inhibitor atomicity/loss, P3-5 the 1,024 MiB reference, and
P3-6 explicit PB paths are closed. P1-1 remains incomplete under a concrete
KWin internal-output counterexample, and P1-3 contains two executable blockers
plus one retry-contract ambiguity.

## Six-disposition result

| v2 disposition | Result | Exact v3 evidence |
| --- | --- | --- |
| P1-1 fail-closed internal identity | **FAIL** | §8.2/R12–R19 omit a KWin-internal connector type; counterexample below |
| P1-2 external-change observation | **PASS** | `observe()` ownership at :238–255; commit-not-overwrite, fallback, debounce, echo, disappearance at :527–569; R20–R26 and physical row at :761–775/:802–812 |
| P1-3 deterministic child-socket activation | **FAIL** | wrong systemd method at :600–607; multi-session restart loop at :629–652; S4/S6 retry contradiction at :613–617/:629–634 |
| P2-4 all-or-nothing inhibitors | **PASS** | acquisition/loss/shutdown contract at :406–441; exact R7–R11 at :725–736 |
| P3-5 1,024 MiB authority | **PASS** | decision 14 at :171–177 and physical matrix at :812–817 |
| P3-6 explicit test paths | **PASS** | brace-free inventory at :267–270 and PB-0/PB-1 at :823–824 |

## P1-A — the identity gate excludes DSI, which KWin classifies as internal

v3 G2 counts only connected `eDP*` and `LVDS*` connectors
(`:481–486`) and R19 deliberately pins that naming rule (`:758–760`). But
KWin v6.6.5's exact `DrmConnector::isInternal()` returns true for LVDS, eDP,
**or DSI** (`src/backends/drm/drm_connector.cpp:202–205`):

`https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/backends/drm/drm_connector.cpp#L202-205`

Minimal counterexample: one connected eDP panel plus one connected DSI panel
and one eligible backlight. G2 sees exactly one internal-class connector and
registers. KWin sees two internal outputs; under the already accepted
internal-flag-only/iteration-order matching semantics, the device can bind the
wrong panel. That is the same fail-open failure P1-1 required v3 to eliminate.

Repair G2 from KWin's actual internal set (at minimum `LVDS*`, `eDP*`, and
`DSI*` for the pinned implementation), and add one-DSI plus mixed-eDP/DSI
counterexamples to R12/R15/R16/R19/R35. Better still, consume a typed injected
inventory whose `internal` classification is derived from the pinned adapter,
not a narrower filename heuristic.

## P1-B — systemd has no `UpdateActivationEnvironment` method

S2 requires one `UpdateActivationEnvironment` call to each of
`org.freedesktop.systemd1` and `org.freedesktop.DBus` (`:600–607`). The D-Bus
daemon does expose
`org.freedesktop.DBus.UpdateActivationEnvironment(a{ss})`. systemd 257 exposes
`org.freedesktop.systemd1.Manager.SetEnvironment(as)` (and
`UnsetAndSetEnvironment(as,as)`), not `UpdateActivationEnvironment`:

- `https://www.freedesktop.org/software/systemd/man/257/org.freedesktop.systemd1.html`
- `https://dbus.freedesktop.org/doc/dbus-specification.html#bus-messages-update-activation-environment`

Following the architecture literally makes the systemd half fail before
activation, so the deterministic sequence is not implementable as written.
Name both exact calls and signatures, await both replies, and add fake-manager
signature/error rows. A helper executable may implement those two calls, but
the service-manager call is still `SetEnvironment`.

## P1-C — two same-user supervisors can stop/restart Power1 forever

The v3 multi-session rule is not merely last-writer-wins. S8 tells every
supervisor that finds a foreign `wayland_binding` to `StopUnit` and replace it
(`:638–644`). S6 says an observed provider exit while the session is active
creates a new generation (`:629–634`). S9 then allows multiple same-user
supervisors to share the one activation environment/name (`:645–649`).

Executable trace: session A binds Power1 to A; session B stops it and binds B;
A observes the exit, starts a new generation, stops B, and binds A; B observes
that exit and repeats. Each takeover resets the per-generation retry bound, so
the churn is unbounded. `wayland_binding` makes the fight visible but does not
make it deterministic or safe.

Choose a stable single-session arbiter (for example a session-lane lease keyed
to an explicitly selected active graphical session), make losers publish typed
unsupported/unavailable without stopping the winner, or use per-session
instances/names. Add a simultaneous-two-supervisor row that proves convergence
and no stop/restart loop, not only a two-generation last-writer assertion.

## P2-A — S4 and S6 disagree on the retry count

S4 says **exactly one activation attempt** per published generation
(`:613–617`), while S6 and R33 require up to three attempts in that same
generation (`:629–634`, `:791–792`). State one initial activation plus at most
two retries (or another single unambiguous count) and pin the exact attempt
sequence. This is bounded wording/test repair, separate from the multi-session
P1.

## Verified closures

- The external-change port now has explicit ownership, teardown, event/poll
  sources, convergence/echo behavior, debounce, last-writer policy, typed
  disappearance, deterministic rows, and a physical end-to-end row. It chooses
  `set_observed_brightness`, preserving KWin adaptive learning.
- The shell key-inhibitor contract now gates all actions on all three held
  locks, releases partial acquisition, unregisters collectively on loss,
  cancels across sleep, and names the required fake-logind rows.
- Every 500 MiB reference is superseded by the accepted 1,024 MiB initial
  aggregate idle PSS ceiling with measure-first language.
- PB-0 and PB-1 use explicit real module/test roots and keep the three PB-0
  commits independently reviewable.

Priya can issue one replacement v4 that carries v3 unchanged except for the
four bounded repairs above. Rereview need not reopen the four dispositions that
pass here. No Power implementation slice should be assigned from v3 as a
whole. This review changed no product/docs/tests/Git state and ran no build,
session, D-Bus, display, power, brightness, inhibitor, or hardware action; the
only network reads were exact upstream primary-source files/specifications.
