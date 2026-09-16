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
constexpr int legacySchemaVersion = 1;
constexpr int schemaVersion = 2;
constexpr int maximumScreens = 32;
constexpr int maximumIcons = 4096;
constexpr int maximumKeyLength = 256;
// A global desktop frame is larger than one output but still bounded; the same
// ceiling guards migration arithmetic against overflowing into nonsense.
constexpr qreal maximumCoordinate = 1000000.0;

QString defaultStoragePath() {
  return QDir(QStandardPaths::writableLocation(
                  QStandardPaths::GenericDataLocation))
      .filePath(QStringLiteral("qindaqt/desktop-icon-layout.json"));
}

QVariantMap point(qreal x, qreal y) {
  return QVariantMap{{QStringLiteral("x"), x}, {QStringLiteral("y"), y}};
}
} // namespace

DesktopIconLayoutStore::DesktopIconLayoutStore(QObject *parent)
    : DesktopIconLayoutStore(defaultStoragePath(), parent) {}

DesktopIconLayoutStore::DesktopIconLayoutStore(QString storagePath,
                                               QObject *parent)
    : QObject(parent), m_storagePath(std::move(storagePath)) {
  load();
}

QVariantMap DesktopIconLayoutStore::position(const QString &layoutKey) const {
  if (!validKey(layoutKey)) {
    return {};
  }
  return m_icons.value(layoutKey).toMap();
}

bool DesktopIconLayoutStore::setPosition(const QString &layoutKey, qreal x,
                                         qreal y) {
  if (!validKey(layoutKey) || !validCoordinate(x) || !validCoordinate(y)) {
    return false;
  }
  if (!m_icons.contains(layoutKey) && m_icons.size() >= maximumIcons) {
    return false;
  }
  const QVariantMap previous = m_icons.value(layoutKey).toMap();
  m_icons.insert(layoutKey, point(x, y));
  if (!save()) {
    // AGENT-GUARD: never report a placement the disk refused. A surface that
    // trusted an unsaved move would show the icon somewhere it will not be
    // after a restart.
    if (previous.isEmpty()) {
      m_icons.remove(layoutKey);
    } else {
      m_icons.insert(layoutKey, previous);
    }
    return false;
  }
  emit changed();
  return true;
}

bool DesktopIconLayoutStore::clearAll() {
  if (m_icons.isEmpty() && m_legacyScreens.isEmpty()) {
    return true;
  }
  const QVariantMap previousIcons = m_icons;
  const QVariantMap previousLegacy = m_legacyScreens;
  m_icons.clear();
  // Arranging the desktop also retires an unmigrated v1 arrangement: keeping
  // it would silently resurrect the old per-output placements on the next
  // migration attempt.
  m_legacyScreens.clear();
  if (!save()) {
    m_icons = previousIcons;
    m_legacyScreens = previousLegacy;
    return false;
  }
  emit changed();
  return true;
}

bool DesktopIconLayoutStore::hasLegacyLayout() const {
  return !m_legacyScreens.isEmpty();
}

bool DesktopIconLayoutStore::migrateLegacyLayout(const QString &keptScreenName,
                                                 qreal originX, qreal originY) {
  if (m_legacyScreens.isEmpty()) {
    return true;
  }
  if (!validKey(keptScreenName) || !validCoordinate(originX) ||
      !validCoordinate(originY)) {
    return false;
  }
  const QVariantMap kept = m_legacyScreens.value(keptScreenName).toMap();
  QVariantMap migrated = m_icons;
  for (auto iconIt = kept.cbegin(); iconIt != kept.cend(); ++iconIt) {
    if (migrated.contains(iconIt.key()) || migrated.size() >= maximumIcons) {
      // A placement already made in the new frame wins over the legacy one.
      continue;
    }
    const QVariantMap local = iconIt.value().toMap();
    const qreal x = local.value(QStringLiteral("x")).toReal() + originX;
    const qreal y = local.value(QStringLiteral("y")).toReal() + originY;
    if (!validCoordinate(x) || !validCoordinate(y)) {
      continue;
    }
    migrated.insert(iconIt.key(), point(x, y));
  }
  const QVariantMap previousIcons = m_icons;
  const QVariantMap previousLegacy = m_legacyScreens;
  m_icons = std::move(migrated);
  m_legacyScreens.clear();
  if (!save()) {
    m_icons = previousIcons;
    m_legacyScreens = previousLegacy;
    return false;
  }
  emit changed();
  return true;
}

void DesktopIconLayoutStore::updateDrag(const QVariantMap &positionsByLayoutKey) {
  QVariantMap accepted;
  for (auto it = positionsByLayoutKey.cbegin();
       it != positionsByLayoutKey.cend(); ++it) {
    if (!validKey(it.key()) || accepted.size() >= maximumIcons) {
      continue;
    }
    const QVariantMap candidate = it.value().toMap();
    bool xOk = false;
    bool yOk = false;
    const qreal x = candidate.value(QStringLiteral("x")).toDouble(&xOk);
    const qreal y = candidate.value(QStringLiteral("y")).toDouble(&yOk);
    if (!xOk || !yOk || !validCoordinate(x) || !validCoordinate(y)) {
      continue;
    }
    accepted.insert(it.key(), point(x, y));
  }
  if (accepted == m_dragPositions) {
    return;
  }
  m_dragPositions = std::move(accepted);
  emit dragChanged();
}

void DesktopIconLayoutStore::endDrag() {
  if (m_dragPositions.isEmpty()) {
    return;
  }
  m_dragPositions.clear();
  emit dragChanged();
}

QVariantMap DesktopIconLayoutStore::dragPosition(const QString &layoutKey) const {
  if (!validKey(layoutKey)) {
    return {};
  }
  return m_dragPositions.value(layoutKey).toMap();
}

bool DesktopIconLayoutStore::isDragging() const {
  return !m_dragPositions.isEmpty();
}

void DesktopIconLayoutStore::load() {
  QFile file(m_storagePath);
  if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
    return;
  }
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
  if (!document.isObject()) {
    return;
  }
  const QJsonObject root = document.object();
  const int version = root.value(QStringLiteral("schemaVersion")).toInt();
  if (version != schemaVersion && version != legacySchemaVersion) {
    return;
  }

  if (version == legacySchemaVersion) {
    const QVariantMap screens =
        root.value(QStringLiteral("screens")).toObject().toVariantMap();
    if (screens.size() > maximumScreens) {
      return;
    }
    QVariantMap accepted;
    for (auto screenIt = screens.cbegin(); screenIt != screens.cend();
         ++screenIt) {
      const QVariantMap positions = screenIt.value().toMap();
      if (!validKey(screenIt.key()) || positions.size() > maximumIcons) {
        return;
      }
      QVariantMap acceptedPositions;
      for (auto iconIt = positions.cbegin(); iconIt != positions.cend();
           ++iconIt) {
        bool xOk = false;
        bool yOk = false;
        const QVariantMap local = iconIt.value().toMap();
        const qreal x = local.value(QStringLiteral("x")).toDouble(&xOk);
        const qreal y = local.value(QStringLiteral("y")).toDouble(&yOk);
        if (!validKey(iconIt.key()) || !xOk || !yOk || !validCoordinate(x) ||
            !validCoordinate(y)) {
          return;
        }
        acceptedPositions.insert(iconIt.key(), point(x, y));
      }
      accepted.insert(screenIt.key(), acceptedPositions);
    }
    m_legacyScreens = std::move(accepted);
    return;
  }

  const QVariantMap icons =
      root.value(QStringLiteral("icons")).toObject().toVariantMap();
  if (icons.size() > maximumIcons) {
    return;
  }
  QVariantMap accepted;
  for (auto iconIt = icons.cbegin(); iconIt != icons.cend(); ++iconIt) {
    bool xOk = false;
    bool yOk = false;
    const QVariantMap stored = iconIt.value().toMap();
    const qreal x = stored.value(QStringLiteral("x")).toDouble(&xOk);
    const qreal y = stored.value(QStringLiteral("y")).toDouble(&yOk);
    if (!validKey(iconIt.key()) || !xOk || !yOk || !validCoordinate(x) ||
        !validCoordinate(y)) {
      return;
    }
    accepted.insert(iconIt.key(), point(x, y));
  }
  m_icons = std::move(accepted);
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
  // An unmigrated legacy arrangement is written back verbatim under its own
  // key so a still-running older shell keeps reading it.
  QJsonObject root{
      {QStringLiteral("schemaVersion"),
       m_legacyScreens.isEmpty() ? schemaVersion : legacySchemaVersion},
      {QStringLiteral("icons"), QJsonObject::fromVariantMap(m_icons)}};
  if (!m_legacyScreens.isEmpty()) {
    root.insert(QStringLiteral("screens"),
                QJsonObject::fromVariantMap(m_legacyScreens));
  }
  if (file.write(QJsonDocument(root).toJson(QJsonDocument::Compact)) < 0) {
    file.cancelWriting();
    return false;
  }
  return file.commit();
}

bool DesktopIconLayoutStore::validKey(const QString &value) {
  return !value.isEmpty() && value.size() <= maximumKeyLength;
}

bool DesktopIconLayoutStore::validCoordinate(qreal value) {
  return std::isfinite(value) && std::abs(value) <= maximumCoordinate;
}

} // namespace QindaQt::Shell::DesktopSurface
