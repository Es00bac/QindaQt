# ADR-0042: Pure Font F0 catalog and preference boundary

- **Status:** Accepted
- **Date:** 2026-08-28
- **Owners:** Faye Lin <faye.lin@qindaqt.local>
- **Supersedes:** None
- **Superseded by:** None

## Context

QindaQt requires deterministic font family discovery and typography preferences
to support Settings1 appearance configuration and QST-1 type scale rendering.
Direct host enumeration or live fontconfig mutations introduce non-deterministic
discovery order, unexpected process side-effects, and tight coupling between
core presentation components and platform font daemon implementations.

A pure, decoupled Qt boundary is needed to discover families deterministically
from injected facts, validate typography preferences (family, monospace family,
point size, antialiasing, hinting, subpixel order, logical DPI), publish atomic
revisions, and provide last-known-good (LKG) fallback on failed refreshes.

## Decision

Implement Font F0 as a pure installed Qt library (`qindaqt_font_preferences`,
aliased as `QindaQt::FontPreferences`) under `src/services/font_preferences/`:

1. **Deterministic Family Discovery**: Build `FontCatalog` from injected `FontFact`
   records with case-insensitive deduplication, whitespace normalization, and
   alphabetical ordering by canonical key. Reject invalid, empty, or conflicting
   facts (such as mismatched monospace flags for the same family).
2. **Validated Preferences**: Provide `FontPreferences` capturing valid family names,
   bounded point sizes (`[4.0, 144.0]`), antialiasing modes (`none`, `grayscale`,
   `subpixel`), hinting (`none`, `slight`, `medium`, `full`), subpixel ordering
   (`none`, `rgb`, `bgr`, `vrgb`, `vbgr`), and optional logical DPI (`[48.0, 576.0]`).
3. **Lossless Codecs**: Provide `FontPreferencesCodec` supporting bidirectional
   round-trip serialization to JSON, `QVariantMap`, and Settings1 schema keys.
4. **Pre-Application Bootstrap**: Provide `FontBootstrap` to derive `QFont`
   and toolkit rendering attributes prior to QML engine and window construction.
5. **Atomic Publication & LKG Retain**: Provide `FontPreferencesCoordinator`
   to track monotonic revisions and preserve last-known-good catalog and preference
   snapshots across invalid refresh or update attempts.

## Consequences

- Font discovery and preference validation remain pure, testable, and isolated
  from D-Bus, KWin, Wayland, and live fontconfig daemon processes.
- Applications and Settings consumers can rely on deterministic family sorting
  and robust fallback behavior.
- Installed public headers can be consumed independently by other modules.

## Revisit when

A future milestone (Font F1 / Font1 D-Bus service) introduces live fontconfig
fact discovery adapters or live D-Bus notification publication.
