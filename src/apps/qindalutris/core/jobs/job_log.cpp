// SPDX-License-Identifier: GPL-3.0-or-later
#include "job_log.h"

#include <QDateTime>

namespace QindaQt::QindaLutris {

namespace {

QString flattened(QString line) {
  for (QChar &c : line) {
    if (c.category() == QChar::Other_Control) {
      c = QLatin1Char(' ');
    }
  }
  if (line.size() > JobLog::kMaxLineChars) {
    line.truncate(JobLog::kMaxLineChars);
    line.append(QStringLiteral(" [...]"));
  }
  return line;
}

} // namespace

JobLog::JobLog(const QString &header) { reset(header); }

void JobLog::reset(const QString &header) {
  m_header = flattened(header);
  m_lines.clear();
  m_dropped = 0;
}

void JobLog::append(const QString &text) {
  const QString stamp =
      QDateTime::currentDateTimeUtc().toString(Qt::ISODate) + QLatin1Char(' ');
  const QStringList parts = text.split(QLatin1Char('\n'));
  for (const QString &part : parts) {
    if (part.trimmed().isEmpty() && parts.size() > 1) {
      continue;
    }
    m_lines.append(stamp + flattened(part));
  }
  while (m_lines.size() > kMaxLines) {
    m_lines.removeFirst();
    ++m_dropped;
  }
}

QString JobLog::text() const {
  QStringList out;
  if (!m_header.isEmpty()) {
    out.append(m_header);
  }
  if (m_dropped > 0) {
    out.append(QStringLiteral("[%1 earlier lines dropped]").arg(m_dropped));
  }
  out.append(m_lines);
  return out.join(QLatin1Char('\n'));
}

} // namespace QindaQt::QindaLutris
