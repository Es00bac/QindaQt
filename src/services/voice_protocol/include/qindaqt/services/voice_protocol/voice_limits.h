// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QtTypes>

namespace QindaQt::Services::Voice {

// AGENT-CONTRACT: every bound here is enforced on decode, not on encode. The
// Voice1 provider is a separate, independently released process (Gabbee); a
// snapshot that exceeds any of these is rejected as malformed rather than
// truncated, so a provider defect can never widen what the shell renders.
inline constexpr quint32 kSchemaVersion = 1;

// Structured, machine-comparable codes only: [a-z0-9-].
inline constexpr qsizetype kMaxReasonCodeUtf8Bytes = 64;

// Stable identifiers the shell may compare or persist: provider ids, language
// tags. Restricted alphabet, see isStructuredIdentifier.
inline constexpr qsizetype kMaxIdentifierUtf8Bytes = 64;

// Human-readable, provider-authored display strings. Never persisted, never
// compared; rendered as plain text with no markup interpretation.
inline constexpr qsizetype kMaxLabelUtf8Bytes = 128;

// Shortcut descriptions such as "F5" or "Meta+Shift+V", for the chip tooltip.
inline constexpr qsizetype kMaxShortcutUtf8Bytes = 64;

// AGENT-GUARD: partial and final transcript text crosses this boundary so the
// panel chip can show what the user is dictating, exactly as Gabbee's own bar
// does. It is display-only, bounded, and never persisted by the shell. Raising
// this bound turns a panel applet into a transcript store; do not raise it
// without an ADR.
inline constexpr qsizetype kMaxTranscriptUtf8Bytes = 512;

inline constexpr quint32 kMaxLevelPercent = 100;

// The provider advertises what a user could switch to. A desktop with more
// than a dozen speech providers installed is not a case worth rendering, and
// an unbounded list would let a provider stall the panel's projection.
inline constexpr qsizetype kMaxProviders = 12;

// One in-flight operation per client; a provider that does not answer within
// this window is reported uncertain and the intent is never replayed.
inline constexpr int kDefaultRequestTimeoutMs = 5'000;

} // namespace QindaQt::Services::Voice
