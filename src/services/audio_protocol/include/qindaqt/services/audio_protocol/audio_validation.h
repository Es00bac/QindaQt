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
[[nodiscard]] ValidationResult validateOperationResult(const OperationResult &result);
[[nodiscard]] bool isBoundedText(const QString &value, qsizetype maxUtf8Bytes);
[[nodiscard]] QString boundedSafeDiagnostic(QString value);

} // namespace QindaQt::Audio
