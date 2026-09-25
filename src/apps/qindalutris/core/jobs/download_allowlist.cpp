// SPDX-License-Identifier: GPL-3.0-or-later
#include "download_allowlist.h"

namespace QindaQt::QindaLutris {

const QStringList &allowedDownloadHosts() {
  // AGENT-NOTE: vendor hosts verified live on 2026-09-25 (see the recipe
  // table in store_recipes.cpp for the exact URLs and their redirects).
  static const QStringList hosts{
      // GE-Proton releases and their release API.
      QStringLiteral("github.com"),
      QStringLiteral("objects.githubusercontent.com"),
      QStringLiteral("release-assets.githubusercontent.com"),
      QStringLiteral("api.github.com"),
      // Battle.net (getInstaller redirects to a path on the same host).
      QStringLiteral("downloader.battle.net"),
      // EA app.
      QStringLiteral("origin-a.akamaihd.net"),
      // Ubisoft Connect.
      QStringLiteral("static3.cdn.ubi.com"),
      // Epic Games Launcher (the API host redirects to the CDN host).
      QStringLiteral("launcher-public-service-prod06.ol.epicgames.com"),
      QStringLiteral("epicgames-download1.akamaized.net"),
      // GOG Galaxy.
      QStringLiteral("webinstallers.gog-statics.com"),
      // Amazon Games.
      QStringLiteral("download.amazongames.com"),
      // Compatibility database refresh (placeholder until decided).
      QString::fromLatin1(kCompatDbRefreshHost),
  };
  return hosts;
}

bool isAllowedDownloadUrl(const QUrl &url) {
  if (!url.isValid() || url.isRelative()) {
    return false;
  }
  if (url.scheme().compare(QLatin1String("https"), Qt::CaseInsensitive) != 0) {
    return false;
  }
  // AGENT-GUARD: `https://good.host@evil.com/` names evil.com; refusing any
  // user-info at all removes the whole class of authority confusion.
  if (!url.userInfo().isEmpty() ||
      url.authority(QUrl::FullyEncoded).contains(QLatin1Char('@'))) {
    return false;
  }
  if (url.port() != -1 && url.port() != 443) {
    return false;
  }
  const QString host = url.host(QUrl::FullyEncoded).toLower();
  return !host.isEmpty() && allowedDownloadHosts().contains(host);
}

bool isAllowedDownloadUrl(const QString &text) {
  for (const QChar c : text) {
    if (c.isSpace() || c == QLatin1Char('\\') || c.category() == QChar::Other_Control) {
      return false;
    }
  }
  return isAllowedDownloadUrl(QUrl(text, QUrl::StrictMode));
}

} // namespace QindaQt::QindaLutris
