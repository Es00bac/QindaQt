// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/clipboard_protocol/clipboard_protocol.h>

namespace QindaQt::Services::Clipboard {

struct ValidationResult {
    bool accepted = false;
    QString reasonCode;
};

[[nodiscard]] bool isStructuredReasonCode(const QString &value);
[[nodiscard]] ValidationResult validateSnapshot(const Snapshot &snapshot);
[[nodiscard]] ValidationResult validateOperationRequest(const OperationRequest &request);
[[nodiscard]] ValidationResult validateOperationResult(const OperationResult &result);

} // namespace QindaQt::Services::Clipboard
