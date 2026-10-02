# ADR-0334: Adapt the live shortcut authority and its native consumers

## Status

Proposed candidate for PF23–PF24; supplements [ADR-0317](0317-native-global-shortcut-registry.md). Runtime selection remains explicitly qualified by packaging, with the source option OFF until accepted tests and live compositor checks pass.

## Context

The pure native registry/store already exists. The earlier native-shortcuts fork candidate provides KF6-compatible codecs and D-Bus methods but lacks actual unique-owner cleanup, concrete legacy import and persistent runtime choices. QindaQt must preserve current shortcut behavior while adapting existing Plasma service logic into its own authority, Settings and portal boundaries.

## Decision

Selectively adapt the existing candidate and KGlobalAccelD v6.6.6 logic, retaining attribution. The fork owns policy/store, input dispatch and wire authority. Separate collaborators implement compatibility codecs/component methods, bus ownership, native methods, KConfig legacy import, rolling sequences/modifier-only dispatch and KService/KIO desktop-file activation. New fork source lives under `qindaqt/shortcuts/`; unrelated current fork behavior is preserved.

`KWIN_NATIVE_SHORTCUTS=ON` removes discovery/linkage of KGlobalAccelD. It preserves the standard `org.kde.kglobalaccel` wire for current KF6 clients and exposes native `org.qindaqt.Shortcuts1` at `/org/qindaqt/Shortcuts1`:

| Method | Signature | Contract |
| --- | --- | --- |
| ListBindings | `→ a{sv}` | Stable component/action ID to row map; row component, action, description, componentLabel, keys/defaults portable `as`, active/repeat booleans |
| Register | `sssasbb → bs` | Component, action, description, keys, repeat, transient; exact applied/conflict result; claims actual unique caller |
| SetShortcuts | `ssas → bs` | Refuses conflicts atomically; user-persistent entries editable by same-UID Settings; transient entries require their exact owner |
| Conflicts | `asss → as` | Exact conflicting stable IDs, excluding supplied component/action; invalid sequence refuses |
| Unregister | `ss → b` | Releases binding; transient entries require their exact owner |

`Activated`/`Deactivated` carry `(component, action, monotonic-ms timestamp)` as `sst`; `BindingsChanged` is empty. One native client module owns this wire and unique-authority signal connection; Settings and GlobalShortcuts consume its public interface, never each other's implementation.

Every call authenticates the actual session-bus unique owner and same effective UID. Friendly labels are presentation only. Runtime actions become inactive on owner loss while keeping persistent reservations; transient portal actions are removed. Desktop-file actions use existing KService/KIO launch parsing and remain independently available. Blocking shortcuts is scoped to live blockers and clears on their loss.

The native JSON store imports default-context `kglobalshortcutsrc` triples once, preserving saved native choices. Startup loads inactive runtime actions until their clients reclaim them. Complete portable sequences and defaults persist through an atomic replace. Failed writes restore registry, action metadata, owner watches and store policy together. Non-default compatibility contexts are explicitly refused; no speculative successful stub claims their behavior.

Input dispatch adapts existing modifier-only, rolling four-chord and press/repeat/release logic. Current compositor layout normalization remains the authority for event keys; lock and shortcut inhibitor admission suppress activation. Pointer/axis activity cancels modifier-only sequences. This change does not redesign gestures.

GlobalShortcuts owns bounded standard Session/Request lifetimes and transient native bindings, adapting the existing frontend/caller/pidfd admission. Its separate shortcut helper edits real key sequences on the exact admitted ordinary display. It uses public Qt widgets, native conflict reads and the existing foreign-parent contract; consent files owned by remote-input adaptation remain independent. Close, caller/frontend/authority/display/lock loss revoke bindings. Metadata routes remain held until acceptance.

## Verification and remaining boundary

Focused private-bus contracts must prove actual KF6 registration/invocation/signatures, unique-owner lifetime, foreign transient refusal, failed-save rollback, native values winning legacy import, repeat/release/modifier-only/multi-key behavior and admission. Consumer tests must prove real native replies, conflict refusal, command-file lifecycle and portal cancellation/owner loss. Compositor build and nested live shortcuts/layout/lock/inhibitor scenarios qualify the selected runtime before daemon/supervisor/recipe retirement. Candidate source and compiler activity add no release acceptance.

The focused candidate fixture compiles the actual native client, Settings adapter, portal policy/session/adaptor/process and separate helper with strict warnings. Its three private-bus CTests pass: native Settings wire and command lifecycle; bounded portal selection policy; and an actual fork authority cross-repository bridge covering multi-key/conflict behavior, Create/Bind/Activate/Deactivate/List/Close, cancelled consent and frontend-loss revocation. This is service-contract evidence; the displayed helper, full composition and nested compositor selection remain separate gates.

## References

- [Input Settings](../apps/input-settings.md)
- [Portal foundation](../architecture/portal-foundation.md)
- [Portal service](../architecture/portal-service.md)
- [Native shortcut registry, ADR-0317](0317-native-global-shortcut-registry.md)
