// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Internal to store_clients.cpp and store_client_parsers.cpp.

#include <QByteArray>
#include <QDir>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QString>
#include <QUrl>

namespace QindaQt::QindaLutris::store_detail {

// AGENT-NOTE: the same shape as isSafeStoreGameId (store_launch.h, model
// library, which this library may not include); tst_store_clients checks
// the two agree.
inline bool isClientGameId(const QString &id) {
  static const QRegularExpression pattern(
      QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$"));
  return pattern.match(id).hasMatch();
}

inline bool isCode(const QString &code) {
  static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9._~-]{8,1024}$"));
  return pattern.match(code).hasMatch();
}

inline constexpr qsizetype kMaxClientJsonBytes = 32 * 1024 * 1024;

// The clients log to stderr, but be tolerant: the whole output, else the
// last line that is a JSON document.
inline QJsonDocument jsonFrom(const QByteArray &out) {
  if (out.size() > kMaxClientJsonBytes) {
    return {};
  }
  QJsonParseError error{};
  QJsonDocument whole = QJsonDocument::fromJson(out.trimmed(), &error);
  if (error.error == QJsonParseError::NoError) {
    return whole;
  }
  const QList<QByteArray> lines = out.split('\n');
  for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
    const QByteArray line = it->trimmed();
    if (line.startsWith('{') || line.startsWith('[')) {
      QJsonDocument doc = QJsonDocument::fromJson(line, &error);
      if (error.error == QJsonParseError::NoError) {
        return doc;
      }
    }
  }
  return {};
}

inline QUrl httpsUrl(const QString &text) {
  const QUrl url(text.startsWith(QLatin1String("//")) ? QStringLiteral("https:") + text : text);
  return url.isValid() && url.scheme() == QLatin1String("https") && !url.host().isEmpty()
             ? url
             : QUrl();
}

// "a\\b/c.exe" under root (leading separators dropped), refusing `..`.
// Empty on refusal.
inline QString joinInside(const QString &root, const QString &relative) {
  QString path = relative;
  path.replace(QLatin1Char('\\'), QLatin1Char('/'));
  // legendary does the same (`.lstrip('/')`): manifests may start with one.
  while (path.startsWith(QLatin1Char('/'))) {
    path.remove(0, 1);
  }
  if (path.isEmpty() ||
      path.split(QLatin1Char('/')).contains(QStringLiteral(".."))) {
    return {};
  }
  return QDir::cleanPath(root + QLatin1Char('/') + path);
}

} // namespace QindaQt::QindaLutris::store_detail
