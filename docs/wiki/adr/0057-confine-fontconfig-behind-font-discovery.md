# ADR-0057: Confine fontconfig behind the Font F1 discovery provider

- **Status:** Accepted
- **Date:** 2026-09-02
- **Owners:** Ruth Teitelbaum <ruth.teitelbaum@qindaqt.local>
- **Supersedes:** None
- **Superseded by:** None

## Context

Font F0 ([ADR-0047](0047-pure-font-catalog-and-preference-boundary.md))
established a pure catalog and preference boundary that consumes injected
`FontFact` values and deliberately never touches live font discovery. Font F1
needs a real producer of those facts: applications must ship with confirmed
typography applied at startup, and the catalog must reflect the fonts actually
installed for the session rather than test fixtures.

fontconfig is the platform authority for font enumeration on the targeted
Linux sessions, but linking it broadly would violate the F0 boundary: its
global-default configuration, cache order, and environment sensitivity would
make discovery non-deterministic and would couple presentation-facing modules
to a platform daemon implementation. The F1 gap recorded on the roadmap named
"a live discovery provider" as a remaining deliverable.

Alternatives considered: (a) enumerate fonts through `QFontDatabase` in each
consumer — rejected, it duplicates discovery logic, offers no injected-config
seam for hostile tests, and spreads platform coupling across modules;
(b) extend `font_preferences` itself with fontconfig — rejected, it would
break the pure F0 boundary that ADR-0047 accepted and that the F0 boundary
gate enforces; (c) a resident Font1 D-Bus service — deferred, no consumer
needs cross-process font authority yet.

## Decision

1. A new module `src/services/font_discovery` (`QindaQt::FontDiscovery`) is
   the sole fontconfig-backed producer of F0 `FontFact` values. It is the only
   module that includes fontconfig headers or links fontconfig; the link is
   PRIVATE so no consumer gains fontconfig include paths or usage
   requirements. The poison gate is `qindaqt.font-discovery-boundary`.
2. The provider never uses ambient state in tests: every discovery request
   carries injected font directories and an injected fontconfig configuration
   file, and the provider builds its `FcConfig` internally from exactly those
   inputs. Only `FontDiscoveryRequest::productionDefault()` resolves the
   default fontconfig configuration, reserved for the production composition.
3. Discovery is fail-closed and bounded: malformed requests, unparsable or
   missing configuration, missing injected directories, and fontconfig
   initialization failures return `available == false` with no facts. Result
   counts and string lengths are contractually bounded; over-long strings
   reject the whole pattern rather than truncating into an aliased family
   identity; ordering is an explicit sort, never fontconfig cache order.
4. `font_preferences` gains the Settings1 persistence composition
   (`FontSettingsBridge`) additively and stays transport-free: no
   `font_preferences` source imports Qt D-Bus, and the F0 boundary gate
   (`qindaqt.font-preferences-boundary`) enforces exactly that.
5. The pre-window bootstrap is the production composition root
   `FontSessionBootstrap` in `font_discovery` (the top of the font module
   DAG: `font_discovery` already depends on `font_preferences`; placing the
   composition there keeps the DAG acyclic). Each first-party application
   (Settings Center, Text Editor, File Manager, Terminal) makes exactly one
   guarded `FontSessionBootstrap::applyFromSessionSettings()` call before
   `QGuiApplication` construction — safe on Qt 6.11 because pre-construction
   `QGuiApplication::setFont()` persists as the application default font and
   blocking D-Bus calls work without an application object (an event loop
   does not, so the read is a bounded blocking exchange on a private
   `connectToBus()` connection; the shared `sessionBus()` is never created
   pre-application). The composition reads the confirmed `fonts.*` snapshot,
   invokes the provider with `FontDiscoveryRequest::productionDefault()`, and
   applies the confirmed typography only when the live catalog resolves the
   family. Qt D-Bus and Qt Gui are PRIVATE dependencies confined to
   `font_session_bootstrap.cpp`, enforced by `qindaqt.font-discovery-boundary`.
   The shell is untouched. A missing or unavailable preference source changes
   nothing.

## Consequences

- fontconfig joins the documented dependency table (README) as a build
  requirement of the discovery provider only.
- Discovery results are deterministic and testable with vendored OFL fixtures
  under private temporary directories; tests never consult the host font
  configuration.
- Consumers keep depending on plain value types; a future Font1 service can
  wrap the provider without changing the F0/F1 value contracts.
- The bridge's per-key commit sequence inherits the ADR-0028 recovery truth
  (no stale base revisions, no uncertain replays, truthful per-key results)
  and can be replaced by a public batch API without touching consumers.
- The bootstrap applies family, point size, hinting, and antialiasing to the
  Qt default font only; monospace family and logical DPI remain persisted
  intent for later consumers.

## Revisit when

- a Font1 D-Bus service or a public multi-key Settings1 transaction API
  lands; or
- a consumer needs live catalog refresh, DPI application, or session-wide
  monospace defaults with an owning boundary.
