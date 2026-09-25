// SPDX-License-Identifier: GPL-3.0-or-later
#include "game.h"

namespace QindaQt::QindaLutris {

QString gameSourceId(GameSource source) {
  switch (source) {
  case GameSource::Steam: return QStringLiteral("steam");
  case GameSource::Lutris: return QStringLiteral("lutris");
  case GameSource::Desktop: return QStringLiteral("desktop");
  case GameSource::Wine: return QStringLiteral("wine");
  case GameSource::Installed: return QStringLiteral("installed");
  }
  Q_UNREACHABLE();
}

std::optional<GameSource> gameSourceForId(const QString &id) {
  if (id == QLatin1String("steam")) return GameSource::Steam;
  if (id == QLatin1String("lutris")) return GameSource::Lutris;
  if (id == QLatin1String("desktop")) return GameSource::Desktop;
  if (id == QLatin1String("wine")) return GameSource::Wine;
  if (id == QLatin1String("installed")) return GameSource::Installed;
  return std::nullopt;
}

QString gameSourceLabel(GameSource source) {
  switch (source) {
  case GameSource::Steam: return QStringLiteral("Steam");
  case GameSource::Lutris: return QStringLiteral("Lutris");
  case GameSource::Desktop: return QStringLiteral("Native");
  case GameSource::Wine: return QStringLiteral("Wine");
  case GameSource::Installed: return QStringLiteral("Installed");
  }
  Q_UNREACHABLE();
}

QString wineRunnerId(WineRunner runner) {
  switch (runner) {
  case WineRunner::Wine: return QStringLiteral("wine");
  case WineRunner::Proton: return QStringLiteral("proton");
  }
  Q_UNREACHABLE();
}

std::optional<WineRunner> wineRunnerForId(const QString &id) {
  if (id == QLatin1String("wine")) return WineRunner::Wine;
  if (id == QLatin1String("proton")) return WineRunner::Proton;
  return std::nullopt;
}

QString normalizedTitleForMatch(const QString &title) {
  QString out;
  out.reserve(qMin(title.size(), kMaxGameTitleChars));
  bool pendingSpace = false;
  for (const QChar ch : title) {
    if (out.size() >= kMaxGameTitleChars) {
      break;
    }
    // Letters and numbers survive, folded; everything else collapses to at
    // most one separator. Decomposed marks are kept by QChar::toCaseFolded
    // as-is -- both sides of a match come through this same transform, so a
    // consistent fold is what matters, not locale-perfect text.
    if (ch.isLetterOrNumber()) {
      if (pendingSpace && !out.isEmpty()) {
        out += QLatin1Char(' ');
      }
      pendingSpace = false;
      out += ch.toCaseFolded();
    } else {
      pendingSpace = true;
    }
  }
  return out;
}

} // namespace QindaQt::QindaLutris
