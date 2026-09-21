// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/voice_protocol/voice_types.h>

namespace QindaQt::Services::Voice {

struct ValidationResult {
    bool accepted = false;
    QString reasonCode;
};

// [a-z0-9-], non-empty, within kMaxReasonCodeUtf8Bytes.
[[nodiscard]] bool isStructuredReasonCode(const QString &value);

// [a-z0-9._-], non-empty, within kMaxIdentifierUtf8Bytes. Provider ids and
// language tags are compared and persisted, so they may not carry whitespace,
// case variance, or control characters.
[[nodiscard]] bool isStructuredIdentifier(const QString &value);

// Provider-authored display text: bounded, and free of every control
// character. Rendered verbatim as plain text with no markup interpretation.
[[nodiscard]] bool isDisplayText(const QString &value, qsizetype maxUtf8Bytes);

// Transcript text additionally admits the line and tab breaks that spoken
// layout ("new paragraph", "tab") legitimately produces. No other control
// character is accepted, so a transcript can never carry terminal escapes.
[[nodiscard]] bool isTranscriptText(const QString &value);

[[nodiscard]] ValidationResult validateSnapshot(const Snapshot &snapshot);
[[nodiscard]] ValidationResult validateOperationRequest(const OperationRequest &request);
[[nodiscard]] ValidationResult validateOperationResult(const OperationResult &result);

// Clamp an unbounded provider level report into the rendered range. Unlike the
// structured payloads, a level is discardable telemetry: clamping is correct
// and rejecting the whole meter would be worse than showing a bounded value.
[[nodiscard]] quint32 clampLevelPercent(quint32 value) noexcept;

} // namespace QindaQt::Services::Voice
