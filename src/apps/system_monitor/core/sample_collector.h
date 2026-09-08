// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "procfs_reader.h"

#include <QVariantList>

#include <optional>

namespace QindaQt::SystemMonitor {

class SampleCollector final {
public:
  explicit SampleCollector(QString procRoot = QStringLiteral("/proc"));

  [[nodiscard]] PublishedSample collect(QString *error = nullptr);
  // Deterministic value seam for parser/rate fixtures; production uses
  // collect().
  [[nodiscard]] PublishedSample publish(RawSample sample);
  void reset();

private:
  ProcfsReader m_reader;
  std::optional<RawSample> m_previous;
  QVariantList m_history;
};

} // namespace QindaQt::SystemMonitor
