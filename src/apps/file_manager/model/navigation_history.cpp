// SPDX-License-Identifier: GPL-3.0-or-later
#include "navigation_history.h"

#include <QDir>
#include <QStringList>

namespace QindaQt::Apps::FileManager {

bool NavigationHistory::hasCurrent() const { return m_current.has_value(); }

const QString &NavigationHistory::currentPath() const {
  static const QString empty;
  return m_current ? *m_current : empty;
}

bool NavigationHistory::canGoBack() const { return !m_back.isEmpty(); }

bool NavigationHistory::canGoForward() const { return !m_forward.isEmpty(); }

void NavigationHistory::reset(QString path) {
  m_back.clear();
  m_forward.clear();
  m_current = std::move(path);
}

bool NavigationHistory::navigateTo(QString path) {
  if (m_current && *m_current == path) {
    return false;
  }
  if (m_current) {
    m_back.append(*m_current);
  }
  m_forward.clear();
  m_current = std::move(path);
  return true;
}

std::optional<QString> NavigationHistory::goBack() {
  if (m_back.isEmpty()) {
    return std::nullopt;
  }
  if (m_current) {
    m_forward.append(*m_current);
  }
  m_current = m_back.takeLast();
  return m_current;
}

std::optional<QString> NavigationHistory::goForward() {
  if (m_forward.isEmpty()) {
    return std::nullopt;
  }
  if (m_current) {
    m_back.append(*m_current);
  }
  m_current = m_forward.takeLast();
  return m_current;
}

std::optional<QString>
NavigationHistory::parentOf(const QString &absolutePath) {
  const QString cleaned = QDir::cleanPath(absolutePath);
  if (cleaned.isEmpty() || cleaned == QDir::rootPath() ||
      cleaned == QLatin1String("/")) {
    return std::nullopt;
  }
  const qsizetype lastSlash = cleaned.lastIndexOf(QLatin1Char('/'));
  if (lastSlash <= 0) {
    return QStringLiteral("/");
  }
  return cleaned.left(lastSlash);
}

QVector<BreadcrumbSegment>
NavigationHistory::breadcrumbFor(const QString &absolutePath) {
  QVector<BreadcrumbSegment> segments;
  const QString cleaned = QDir::cleanPath(absolutePath);
  if (cleaned.isEmpty()) {
    return segments;
  }
  segments.append({QStringLiteral("/"), QStringLiteral("/")});
  const QStringList parts = cleaned.split(QLatin1Char('/'), Qt::SkipEmptyParts);
  QString accumulated;
  for (const QString &part : parts) {
    accumulated += QLatin1Char('/') + part;
    segments.append({part, accumulated});
  }
  return segments;
}

} // namespace QindaQt::Apps::FileManager
