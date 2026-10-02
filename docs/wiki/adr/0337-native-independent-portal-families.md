# ADR-0337: Native independent portal families

- Status: Proposed, pending exact candidate review and executable gates
- Date: 2026-10-02

## Context

Print, Account, DynamicLauncher and USB still route to the external KDE backend. Their installed freedesktop backend contracts are independent of capture and input control. The existing foundation already supplies Request lifetime, frontend authentication, native selected-session attachment and lock authority.

## Decision

Add one per-family adapter/policy under `misc_families`, borrowing the existing public request/session/consent boundaries. Reuse the ordinary FD-bound Qt Widgets helper pattern for presentation and retain the complete upstream Qt/CUPS print conversions with credits. Keep those conversions outside the resident. The only shared boundary addition is an explicit helper executable at composition; old constructors provide no ambient helper fallback.

Print preparation tokens bind to app and frontend, expire and are consumed once. Print data cross as a duplicated readable file descriptor and fixed argument/stdin command flow. Account returns consented trusted native data. USB returns consented IDs/access options while the frontend opens devices. DynamicLauncher preparation and noninteractive authorization respect the backend/frontend installation boundary.

## Consequences

Qt PrintSupport's existing private CUPS conversion contract is required by the helper; no new KDE dependency or generic portal framework is introduced. Cancellation retires pending work and kills helper/spool children; already submitted physical jobs cannot be recalled. Tests inject account data, USB offers and a print runner instead of touching real hardware. Route changes require passing method gates and manager review. [Family contract](../reference/portal-misc-families.md) defines the bounds and focused test matrix; [foundation](../architecture/portal-foundation.md) remains actor/lifecycle authority.
