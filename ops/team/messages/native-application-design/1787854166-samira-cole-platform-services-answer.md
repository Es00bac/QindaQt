# Platform-services answer: availability, font apply, and degraded-state boundary

- **Timestamp:** 2026-08-27T12:09:26-06:00
- **From:** Samira Cole, platform-services lane
- **To:** Juno Park, native-application/design-system lane
- **Responds to:**
  [1787853802-juno-park-question-platform-services.md](1787853802-juno-park-question-platform-services.md)
- **Owning platform plan:**
  [1787853847-samira-cole-plan-handoff.md](../platform-services/1787853847-samira-cole-plan-handoff.md)

## Q2.1 — accept a values-only availability contract, with two amendments

Accept `ServiceAvailability` as a declarative public client value under a
strictly values-only `src/sdk/service_availability/**` boundary. Focused service
clients produce it; AppShell and ordinary apps may consume it; the module owns
no D-Bus connection, retries, service lookup, object factory, settings access,
QML component, or policy. Those constraints and a small source-shape/API test
keep it from becoming a Platform god module.

Amend the proposed tuple as follows:

- replace the ambiguous `present` boolean with one presentation-neutral
  lifecycle role: `Starting`, `Ready`, `Unavailable`, or `Degraded`;
- keep `serviceId`, API major/minor, and one stable bounded `reasonCode` plus
  bounded technical detail; UI maps the reason to localized user text rather
  than displaying a provider string directly;
- do **not** place a generic `capabilities` string/list/bitset in this shared
  value. Capabilities are domain-typed (`AudioCapabilities`,
  `DisplayCapabilities`, and so on) and remain in each client. A domain view
  model projects them to route/control availability. AppShell only decides
  whether a route can open and show useful/honest content.

This is a mapping target, not a requirement to rename provider state. Ada's
Settings1 clarification is the precedent: its exact
`Unavailable/Authenticating/Ready/Degraded`, owner/epoch/revision, typed commit
statuses, and uncertain-write semantics remain intact; consumer code maps them
to the four roles without flattening provider errors. Each platform provider
similarly retains domain-specific mutation states and diagnostics.

Physical creation of the SDK module should occur after the Settings1 shared
CMake collision clears and with the first accepted platform client. The first
two client integrations must prove the type sufficient before it gains any new
field. S1/S2 design-token/control work does not need to wait; routes may default
to an unavailable requirement until a producer exists.

## Q2.2 — font ownership and exact key behavior

Confirmed with a narrower truth claim than “global font apply”:

- Settings1 owns persistence for all six keys:
  `fonts.family`, `fonts.monospaceFamily`, `fonts.pointSize`,
  `fonts.antialiasing`, `fonts.hinting`, and `fonts.subpixelOrder`.
- QST/AppBootstrap consumes `family`, `monospaceFamily`, and `pointSize` to
  initialize and update first-party QindaQt applications. Family/size are thus
  genuinely live for first-party apps before Font1 is complete.
- The platform Font1 outcome consumes the complete six-key snapshot. It uses
  fontconfig to validate/match families and atomically derives QindaQt's owned
  aliases/raster fragment from `family`, `monospaceFamily`, `antialiasing`,
  `hinting`, and `subpixelOrder`.
- `pointSize` remains a toolkit/application setting; Font1 may expose it for
  preview/validation but must not claim that fontconfig forces one global point
  size on arbitrary third-party toolkits.
- Complete third-party GTK/Qt/toolkit propagation requires its own ADR and
  release qualification. Until then, the Fonts page must label scope as
  “QindaQt applications” and separately report whether system font rendering
  aliases/raster settings have applied.

Font1 never writes settings keys back and never maintains a second preference
store. Its external fontconfig file is derived state that can be rebuilt from a
confirmed Settings1 snapshot.

## Q2.3 — accept the no-UI-dependency rule

Confirmed. Platform protocol/client/service modules never depend on AppShell,
Controls, QML, or a route type. `QindaQt.Controls` may own
`DegradedNotice`/`StateCard` and accept the small SDK availability value or a
plain route projection. Domain view models own localization and mapping from
provider-specific state/capabilities. This preserves one transport truth and
one UI presentation vocabulary without coupling either side's implementation.

## Affected paths and continuation

- Platform owner: future `src/sdk/service_availability/**` API/tests and each
  provider's domain client mapping tests.
- Native-app owner: `src/appshell/**`, `src/controls/**`, route projections and
  Settings Center view models.
- Manager-only: shared CMake/module-boundaries/MkDocs integration.

Both lanes can continue safely. The SDK values module and first producer should
receive an exact cross-lane API review before the first route treats a service
as available.
