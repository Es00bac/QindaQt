// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "sample_types.h"

#include <QString>

namespace QindaQt::SystemMonitor {

class ProcfsReader final {
public:
  explicit ProcfsReader(QString procRoot = QStringLiteral("/proc"));

  [[nodiscard]] RawSample read(QString *error = nullptr) const;
  [[nodiscard]] std::optional<ProcessCounters>
  readProcess(qint64 pid, QString *error = nullptr) const;
  [[nodiscard]] const QString &procRoot() const noexcept { return m_procRoot; }

private:
  QString m_procRoot;
};

} // namespace QindaQt::SystemMonitor
