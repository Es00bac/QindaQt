// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_protocol/display_types.h>

namespace QindaQt::Display
{

struct ValidationResult {
    bool accepted = false;
    QString reasonCode;

    friend bool operator==(const ValidationResult &, const ValidationResult &) = default;
};

// Pure validation borrows values for one call, returns owned typed failures,
// and is reentrant on any thread. It performs bounded protocol semantics only;
// topology candidate policy belongs to display_topology.
[[nodiscard]] bool isBoundedText(const QString &value, qsizetype maximumUtf8Bytes);
[[nodiscard]] ValidationResult validateMode(const Mode &mode);
[[nodiscard]] ValidationResult validateOutput(const Output &output);
[[nodiscard]] ValidationResult validateCandidate(const Candidate &candidate);
[[nodiscard]] ValidationResult validateTransactionSummary(const TransactionSummary &summary);
[[nodiscard]] ValidationResult validateSnapshot(const Snapshot &snapshot);
[[nodiscard]] ValidationResult validateOperationResult(const OperationResult &result);
[[nodiscard]] ConfirmationRequirement confirmationRequirement(ChangeClass changeClass);

} // namespace QindaQt::Display
