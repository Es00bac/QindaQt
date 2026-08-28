# ADR-0028: Compose the Appearance settings route through Settings1 and QST-1

- **Status:** Accepted
- **Date:** 2026-08-28
- **Owners:** First-party settings (appearance route)
- **Supersedes:** None
- **Superseded by:** None

## Context

The Settings Center currently serves one fixed `notifications` route backed by
the public Settings1 client and its DND-scoped controller
([ADR-0012](0012-persist-notification-quieting-through-settings1.md)). The next
domain is Appearance: installed QST theme selection, a dark/light/system
scheme preference, font family/size, antialiasing/hinting/subpixel policy,
wallpaper choice/mode, and a logical UI scale intent. Three constraints shape
the design:

- The active Settings1 client exposes only single-key optimistic writes; the
  wire supports 1–64 key atomic transactions, but no public batch API exists
  (Settings1 owner answer, `ops/team/messages/native-application-design/1787853958-ada-ruiz-settings1-answer.md`).
- QST-1 ([ADR-0013](0013-own-qst1-semantic-tokens.md)) requires application
  composition — not tokens, not QML — to select themes and publish complete
  token generations.
- Appearance is a preference surface: the slice must not mutate the
  compositor, displays, fonts, or the running session; UI scale application
  remains with public Display1/Settings consumers.

Alternatives considered: (a) invent a multi-key batch by calling the Qt
transport directly — rejected, it bypasses a public boundary; (b) save each
control immediately like the DND toggle — rejected for this domain, because
appearance intent is coherent (a font change plus smoothing policy is one
editing gesture) and a draft/apply/cancel flow matches user expectations;
(c) wait for a public batch API — rejected, the S0 outcome is deliverable
without it.

## Decision

1. The Appearance domain lives in `src/apps/settings/appearance` as a
   self-contained module: validated appearance values, a pure QST-1 preview
   projection, one Settings1-backed route model, and a
   `QindaQt.SettingsApp.Appearance` QML page. The `qindaqt-settings`
   executable owns only the additive route seam (route parsing, per-route
   client scope, and token-facade composition); the notifications route is
   untouched.
2. The route model applies a dirty draft as a **sequence of single-key
   optimistic commits** in fixed key order. It reports truthful per-key
   outcomes and never presents the sequence as one atomic transaction. After
   every commit reply the model waits for the client's fresh authoritative
   snapshot before writing the next key, so no write carries a stale base
   revision. The accepted recovery contract is unchanged: owner loss forbids
   writes, uncertain writes are never replayed automatically, and conflicts
   require explicit user intent against a fresh baseline.
3. Theme resolution is explicit: the configured theme id wins when installed;
   otherwise the built-in dark/light theme matching the scheme preference
   (`system` follows the platform color scheme) is previewed, clearly labeled
   as a fallback; otherwise the first installed theme is used. Resolution is
   total and deterministic.
4. Three additive keys extend schema v2 in place — `appearance.colorScheme`,
   `appearance.wallpaperMode`, and `appearance.uiScale` — with defaults and
   enum/bound constraints. The schema version stays 2: existing persisted
   documents remain valid because layers are partial and validated per key,
   and the Settings1 settings-schema version is lineage metadata, not a wire
   break.
5. The route publishes exactly one complete QST generation (draft-derived)
   to the engine-owned `Tokens` singleton before page QML loads and on every
   draft/confirmed change. QML remains read-only over tokens and never
   touches the settings client.

## Consequences

- A later public batch API can replace the per-key sequence behind the same
  route-model boundary; the page and wiki truth table stay valid as long as
  per-key outcomes remain truthful during the transition.
- Appearance consumers (shell composition, first-party apps) read the same
  Settings1 keys; this slice stores intent only and applies nothing.
- The additive v2 keys require the shipped-schema tests and the appearance
  focused tests to stay green together; key names are a public contract
  mirrored in `AppearanceKeys`.
- Font family remains free text: font discovery/application is a separate
  platform outcome, so this route neither enumerates nor applies host fonts.
- Verification: `qindaqt.appearance-values`, `qindaqt.appearance-preview`,
  `qindaqt.appearance-settings-model`, `qindaqt.appearance-page`, plus the
  existing settings-app and Settings1 suites; documentation gates as usual.

## Revisit when

- A public multi-key transaction API lands on the settings client;
- font discovery/application or wallpaper application gains an owning
  service boundary; or
- the Settings Center grows a shared navigation shell that absorbs route
  composition from the executable.
