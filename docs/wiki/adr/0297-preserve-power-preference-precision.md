# ADR-0297: Preserve power preference precision and disabled actions

- **Status:** Accepted
- **Date:** 2026-09-29
- **Owners:** Native power policy and Settings service
- **Supersedes:** The duration and admitted-action restrictions in [ADR-0293](0293-settings1-native-power-policy-and-powerdevil-import.md)
- **Superseded by:** None

## Context

PowerDevil 6.6.6 stores dim, display-off, and idle-suspend timeouts in seconds.
A whole-minute native schema drops valid choices such as a 30-second dim
interval or a 901-second suspend interval. Its critical action can be disabled,
its lid action can shut down, and SleepMode distinguishes suspend-to-RAM,
hybrid suspend, and suspend-then-hibernate. Migration must preserve those
choices before native runtime policy replaces PowerDevil.

## Decision

The unreleased per-source idle duration keys use the suffix Seconds and admit
integers from 0 to 14400. Existing global power.idleDisplayOffMinutes remains
compatible and converts to seconds. New native overrides win independently
for duration and enabled state. A disabled old global preference does not
force a zero duration over an explicitly re-enabled new per-source preference.

Critical actions admit none, and lid actions admit power-off. Each source has
power.sleep.<source>.mode with suspend, hybrid-sleep, and
suspend-then-hibernate values, mapped from PowerDevil SleepMode 1, 2, and 3.
Unknown enum values remain absent. The source of these mappings remains the
PowerDevil 6.6.6 KConfig definitions and daemon/powerdevilenums.h cited by
ADR-0293; both current configuration definitions use powerdevilrc.

Read at most one MiB and reject invalid UTF-8 or embedded NUL. KConfig parses
a private temporary copy of that exact bounded snapshot because its path API
reopens files. The original user configuration is never modified. This file
contains power preferences only, and the temporary copy is removed on exit.
The Settings1 atomic values-and-marker transaction is unchanged.

## Consequences

- Disabled actions and sub-minute preferences survive migration.
- Runtime workers must implement the documented sleep variants and actions.
- This refinement changes only new, unreleased per-source keys; the deployed
  global compatibility keys retain their units and names.
- Oversized or malformed configuration remains retryable without a marker.

## Verification

Focused import tests cover disabled critical action, shutdown-on-lid,
sub-minute and non-minute intervals, all sleep modes, native override
precedence, schema validation, and oversized input rejection. Settings1
lifecycle and repository gates cover service-name ownership and atomic save.
