// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_surface/desktop_icon_layout_store.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <cmath>
#include <utility>

namespace QindaQt::Shell::DesktopSurface {
namespace {
constexpr int schemaVersion = 1;
constexpr int maximumScreens = 32;
constexpr int maximumIconsPerScreen = 4096;
constexpr int maximumKeyLength = 256;
constexpr qreal maximumCoordinate = 1000000.0;

QString defaultStoragePath() {
  return QDir(QStandardPaths::writableLocation(
                  QStandardPaths::GenericDataLocation))
      .filePath(QStringLiteral("qindaqt/desktop-icon-layout.json"));
}
} // namespace

DesktopIconLayoutStore::DesktopIconLayoutStore(QObject *parent)
    : DesktopIconLayoutStore(defaultStoragePath(), parent) {}

DesktopIconLayoutStore::DesktopIconLayoutStore(QString storagePath,
                                               QObject *parent)
    : QObject(parent), m_storagePath(std::move(storagePath)) {
  load();
}

QVariantMap DesktopIconLayoutStore::position(const QString &screenName,
                                             const QString &layoutKey) const {
  if (!validKey(screenName) || !validKey(layoutKey)) {
    return {};
  }
  return m_screens.value(screenName).toMap().value(layoutKey).toMap();
}

bool DesktopIconLayoutStore::setPosition(const QString &screenName,
                                         const QString &layoutKey, qreal x,
                                         qreal y) {
  if (!validKey(screenName) || !validKey(layoutKey) || !std::isfinite(x) ||
      !std::isfinite(y) || std::abs(x) > maximumCoordinate ||
      std::abs(y) > maximumCoordinate) {
    return false;
  }
  QVariantMap screen = m_screens.value(screenName).toMap();
  if (!m_screens.contains(screenName) && m_screens.size() >= maximumScreens) {
    return false;
  }
  if (!screen.contains(layoutKey) && screen.size() >= maximumIconsPerScreen) {
    return false;
  }
  screen.insert(layoutKey, QVariantMap{{QStringLiteral("x"), x},
                                       {QStringLiteral("y"), y}});
  m_screens.insert(screenName, screen);
  return save();
}

bool DesktopIconLayoutStore::clearScreen(const QString &screenName) {
  if (!validKey(screenName)) {
    return false;
  }
  if (!m_screens.remove(screenName)) {
    return true;
  }
  return save();
}

void DesktopIconLayoutStore::load() {
  QFile file(m_storagePath);
  if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
    return;
  }
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
  const QJsonObject root = document.object();
  if (!document.isObject() ||
      root.value(QStringLiteral("schemaVersion")).toInt() != schemaVersion) {
    return;
  }
  const QVariantMap screens =
      root.value(QStringLiteral("screens")).toObject().toVariantMap();
  if (screens.size() > maximumScreens) {
    return;
  }
  QVariantMap accepted;
  for (auto screenIt = screens.cbegin(); screenIt != screens.cend();
       ++screenIt) {
    const QVariantMap positions = screenIt.value().toMap();
    if (!validKey(screenIt.key()) || positions.size() > maximumIconsPerScreen) {
      return;
    }
    QVariantMap acceptedPositions;
    for (auto iconIt = positions.cbegin(); iconIt != positions.cend();
         ++iconIt) {
      const QVariantMap point = iconIt.value().toMap();
      bool xOk = false;
      bool yOk = false;
      const qreal x = point.value(QStringLiteral("x")).toDouble(&xOk);
      const qreal y = point.value(QStringLiteral("y")).toDouble(&yOk);
      if (!validKey(iconIt.key()) || !xOk || !yOk || !std::isfinite(x) ||
          !std::isfinite(y) || std::abs(x) > maximumCoordinate ||
          std::abs(y) > maximumCoordinate) {
        return;
      }
      acceptedPositions.insert(
          iconIt.key(),
          QVariantMap{{QStringLiteral("x"), x}, {QStringLiteral("y"), y}});
    }
    accepted.insert(screenIt.key(), acceptedPositions);
  }
  m_screens = std::move(accepted);
}

bool DesktopIconLayoutStore::save() const {
  const QFileInfo target(m_storagePath);
  if (!QDir().mkpath(target.absolutePath())) {
    return false;
  }
  QSaveFile file(m_storagePath);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  const QJsonObject root{
      {QStringLiteral("schemaVersion"), schemaVersion},
      {QStringLiteral("screens"), QJsonObject::fromVariantMap(m_screens)}};
  if (file.write(QJsonDocument(root).toJson(QJsonDocument::Compact)) < 0) {
    file.cancelWriting();
    return false;
  }
  return file.commit();
}

bool DesktopIconLayoutStore::validKey(const QString &value) {
  return !value.isEmpty() && value.size() <= maximumKeyLength;
}

} // namespace QindaQt::Shell::DesktopSurface
