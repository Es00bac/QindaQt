// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_protocol/audio_types.h>

#include <QtCore/QString>

namespace QindaQt::Audio
{

struct ValidationResult {
    bool accepted = false;
    QString reasonCode;
};

[[nodiscard]] ValidationResult validateSnapshot(const Snapshot &snapshot);
// The console slice's admission gate (ADR-0173). Exposed separately so a
// producer can check what it is about to publish without building a whole
// snapshot around it.
[[nodiscard]] ValidationResult validateConsole(const Console &console);
[[nodiscard]] ValidationResult validateOperationResult(const OperationResult &result);
[[nodiscard]] bool isBoundedText(const QString &value, qsizetype maxUtf8Bytes);
[[nodiscard]] QString boundedSafeDiagnostic(QString value);

} // namespace QindaQt::Audio
