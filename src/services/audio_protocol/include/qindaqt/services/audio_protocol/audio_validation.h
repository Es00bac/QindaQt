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
// True when the operation names a device or stream by HANDLE in `primary`, and
// so must be checked against the retained snapshot's lineage before anything
// else looks at it.
//
// AGENT-CONTRACT: one definition, used by both the client's preflight and the
// service's admission. A console operation addresses a strip or bus by its
// stable console id and carries no handle at all; checking it as though it did
// rejects the entire console surface as stale before it reaches the console
// model. `SetBusTarget` is the single console kind that also carries a device.
[[nodiscard]] bool operationTargetsHandle(OperationKind kind) noexcept;
[[nodiscard]] bool isBoundedText(const QString &value, qsizetype maxUtf8Bytes);
[[nodiscard]] QString boundedSafeDiagnostic(QString value);

} // namespace QindaQt::Audio
