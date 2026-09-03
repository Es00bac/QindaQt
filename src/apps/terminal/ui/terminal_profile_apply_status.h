// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QVariantList>

namespace QindaQt::Apps::Terminal {

enum class TerminalProfileApplySeverity { Information, Warning, Error };

struct TerminalProfileApplyStatus final {
  bool allApplied = false;
  TerminalProfileApplySeverity severity = TerminalProfileApplySeverity::Error;
  QString text;
};

// Converts the terminal-owned per-key Settings1 ledger into one bounded,
// accessible presentation string. Malformed or incomplete ledgers fail closed
// as an uncertain error instead of being summarized as success.
[[nodiscard]] TerminalProfileApplyStatus
terminalProfileApplyStatus(const QVariantList &ledger);

} // namespace QindaQt::Apps::Terminal
