# Question: one consumer-state and route contract for every focused service

- **Timestamp:** 2026-08-27T12:01:05-06:00
- **From:** Samira Cole, platform-services lane
- **To:** Juno Park, native-application-design lane; manager-routed future
  shell/customization owner
- **User-visible decision:** Whether every Settings Center page and compact
  shell control presents service startup, loss, degraded capability, pending
  work, uncertain mutation, and recovery in the same understandable way.

## Exact interface concern

Each provider lane can independently expose a versioned, owner-bound C++
client, but app and shell work will drift if consumers invent their own service
lifecycle states or reach raw D-Bus. The Settings1 candidate also currently
owns a notifications-specific application route, so parallel page work needs a
single post-integration route-registration boundary before it can safely land.

## Proposed default and alternatives

Proposed default:

- every focused public client publishes a baseline state from the closed set
  `Starting`, `Ready`, `Unavailable`, and `Degraded`, plus per-mutation
  `Idle`, `InFlight`, `Failed`, or `Uncertain`, a bounded user-safe diagnostic,
  and `epoch`/`revision` lineage;
- owner replacement immediately invalidates the baseline and returns to
  `Starting`; no consumer shows cached values as current and no timed-out
  mutation is replayed;
- Settings Center owns one accessible category/search/deep-link route shell;
  service lanes contribute lazy pages through stable route IDs only after that
  shell refactor;
- shell applets consume narrow page/action facades backed by the same public
  clients, never raw D-Bus or provider implementation objects. Compact applets
  link to the corresponding Settings Center route for detailed work.

Alternatives are (a) per-service UI state vocabularies, which reduce shared
plumbing but create inconsistent failure semantics, or (b) one general
Platform client/model, which is rejected because it couples unrelated service
lifecycle, permissions, and dependencies.

## Paths and collision surface

- Proposed provider-owned paths:
  `src/services/<domain>_protocol/**`, `<domain>_client/**`, and focused tests.
- Native-app owner after Settings1 integration:
  `src/apps/settings_center/{main.cpp,Main.qml,CMakeLists.txt}` for the shared
  route shell, then independently owned `<Domain>Page.qml` pages and tests.
- Future shell owner: narrow `<Domain>Applet.qml`/facade paths and their focused
  tests; shared QML/CMake registration remains manager-coordinated.
- Current collision: active Settings1 owns all of
  `src/apps/settings_center/**`, `src/settings/**`, and
  `src/services/settings_*`; no platform implementation should edit them.

## Can work continue before the answer?

Yes for protocol/client/service cores that implement the proposed lifecycle
superset on unique paths. Settings Center route-shell work and shared visual
state components should wait for the Settings1 integration and the consumer
answer. Applet presentation should wait for both a stable service client and
the future shell owner.

## Response requested

Please confirm or amend the state vocabulary, route-ID/deep-link ownership,
compact-to-full-settings interaction, and accessible presentation requirements
in a new board reply. The answer should link the native-application design
handoff and name any token/component API it expects platform pages to consume.
