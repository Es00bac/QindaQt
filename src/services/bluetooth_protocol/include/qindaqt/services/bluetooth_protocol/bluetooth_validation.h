// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_protocol/bluetooth_types.h>

namespace QindaQt::Bluetooth
{

struct ValidationResult {
    bool accepted = false;
    QString reasonCode;
};

// Returns true when text is free of embedded nulls and fits maxUtf8Bytes when
// encoded as UTF-8.
[[nodiscard]] bool isBoundedText(const QString &value, qsizetype maxUtf8Bytes);

// True only for canonical Bluetooth addresses: six uppercase hex digit pairs
// separated by colons (for example "AA:BB:CC:11:22:33"). Bluetooth1 stores and
// publishes addresses only in this form and rejects every other spelling.
[[nodiscard]] bool isCanonicalAddress(const QString &value);

// True for reason tokens used across the wire: 1..kMaxReasonCodeUtf8Bytes
// UTF-8 bytes from [a-z0-9-]. Diagnostics may carry more text but must be
// control-character-safe; both rules keep the fail-closed contract testable.
[[nodiscard]] bool isStructuredReasonCode(const QString &value);

// Sanitizes and truncates a diagnostic to the protocol bound without splitting
// a UTF-8 sequence. Control characters other than newline/tab become spaces.
[[nodiscard]] QString boundedSafeDiagnostic(QString value);

[[nodiscard]] ValidationResult validateSnapshot(const Snapshot &snapshot);
[[nodiscard]] ValidationResult validateOperationResult(const OperationResult &result);
[[nodiscard]] ValidationResult validateOperationRequest(const OperationRequest &request);

} // namespace QindaQt::Bluetooth
