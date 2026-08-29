# Manager next native-app outcome: AppShell S3

- **Timestamp:** 2026-08-27T15:21:06-06:00
- **From:** Manager
- **To:** future AppShell implementer and exact-candidate reviewer
- **State:** queued after accepted Controls S2 integration; no implementation,
  branch, or worker liveness is claimed
- **Required base:** the future exact public milestone containing accepted
  QST-1, Settings1, Audio1, and `QindaQt.Controls 1.0`
- **Owning design:**
  [Juno Park's first-party design](1787853515-juno-park-design-handoff.md),
  especially sections 4, 5, 8–12

## User-visible outcome

Every first-party QindaQt application can use one compiled and installed
`QindaQt.AppShell 1.0` module for ordinary top-level application chrome,
ordered searchable routes, responsive sidebar/rail/hamburger navigation,
keyboard-complete page activation, honest unavailable routes, and a bounded
content column. The same route registry drives navigation and search; an app
must not maintain a second list in QML.

## Boundary

Own only `src/appshell/**`, `tests/appshell/**`, the new primary wiki page
`docs/wiki/apps/app-shell.md`, deterministic fixtures/baselines, and the
smallest additive source/test/MkDocs registries. Depend only on public
`QindaQt.Controls 1.0`, QST-1, and Qt Quick/Controls2/Layouts plus the minimum
Qt Core/QML support needed for immutable route values and registry ownership.

Do not edit or import Settings1, platform-service clients, shell internals,
LayerShellQt, KWin, Kirigami, `src/apps/settings_center/**`, applet hosts, or
profile persistence. The AppShell window is an ordinary Wayland top-level. A
window-state persistence hook may be injected, but S3 owns no storage, D-Bus
transport, singleton process policy, or service availability probe. Live
availability plumbing and Settings pages remain S4/S6.

## Required contracts

1. A fixed route value carries a stable bounded ID, localized title, icon
   name, bounded search keywords, declarative availability, and a page
   component/factory boundary without embedding an instantiated page.
2. The registry is application-owned, ordered and insert-only after startup.
   Empty/invalid fields and duplicate IDs fail registration atomically and
   loudly; consumers never observe a partial route.
3. Search uses the registry's title and keyword projection, preserves registry
   order, has deterministic locale-safe matching, and exposes an explicit
   no-results state. Search and navigation cannot disagree about availability.
4. Availability distinguishes `Available` from a named requirement. It never
   invents a working page: activation of an unavailable route presents an
   accessible `DegradedNotice` naming the requirement and performs no factory
   or transport work.
5. The responsive shell uses QST/Controls only: ordinary sidebar above 900
   logical pixels, icon rail from 640–899, hamburger navigation below 640,
   single-column content where required, minimum 560×420, and an honest
   scrollable/no-crash state at 400×300. The ordinary content column is capped
   at 720 logical pixels and centered when space permits.
6. Forward/reverse Tab, arrow navigation, Enter/Space activation, Escape/Back,
   search focus/clear, focus restoration after route changes, and RTL visual
   order are explicit and testable. No operation is pointer-only; current
   route, unavailable state, search result count, and navigation controls have
   accessible names, roles, descriptions, and state independent of color.
7. Ownership, lifetime, GUI-thread affinity, registration errors, page factory
   lifetime, persistence-hook failure, and 1.0 compatibility expectations are
   documented at the public boundary. AppShell does not retain destroyed page
   or application objects.

## Acceptance evidence

- Unit/property tests cover route bounds, ordering, atomic duplicate/invalid
  rejection, search/title/keyword behavior, availability, no-results, factory
  laziness and destruction, and injected persistence-hook failures.
- Offscreen production-QML tests cover complete forward/reverse focus order,
  arrows, activation, Back/Escape, search, focus restoration, unavailable
  activation, RTL, long localization, reduced motion/transparency, and
  accessible roles/states without mutating QML from the harness.
- Deterministic reviewed application-window fixtures cover A1–A8 from the
  owning design (1080p, WUXGA, 1440p, truthful 125%/150%, 720×480 and
  560×420), plus the A9 400×300 scrollable/no-crash stress row. All five Qinda
  themes must use the same source path with no theme IDs or palette literals.
- The renderer verifies actual DPR and captured pixel dimensions before any
  scale row can pass. Pinned fonts, locale, animation state and software
  rendering keep baselines deterministic; intentional image changes require
  human review.
- Compiled QML, `all_qmllint`, installed-import consumer, source policy,
  strict-warning Debug/Release focused and broad registries, strict wiki/link/
  navigation/source-shape/whitespace gates, and process cleanup all pass.
- Measure AppShell-plus-Controls PSS over the accepted Controls baseline and
  cold first-interactive-frame/route-switch timing. Record the evidence; do not
  manufacture a machine-independent threshold from one host.
- A different worker reviews the exact committed candidate before manager
  integration, followed by the combined public-tree gates.

S3 is complete only when an ordinary first-party application can consume the
installed module and exercise this entire boundary. A static mockup, QML file
loaded from the source tree, or route list duplicated by a fixture is not
acceptance evidence.
