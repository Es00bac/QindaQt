// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <qqmlintegration.h>

namespace QindaQt::Shell::DesktopSurface {

// Owns the bounded, per-output placement of Desktop-directory icons. The
// filesystem identity key is supplied by DesktopContentsController, so a
// rename does not detach an icon from the position the user chose.
// Not final: QML_ELEMENT instantiates through QQmlElement.
class DesktopIconLayoutStore : public QObject {
  Q_OBJECT
  QML_ELEMENT

public:
  explicit DesktopIconLayoutStore(QObject *parent = nullptr);
  explicit DesktopIconLayoutStore(QString storagePath,
                                  QObject *parent = nullptr);

  Q_INVOKABLE QVariantMap position(const QString &screenName,
                                   const QString &layoutKey) const;
  Q_INVOKABLE bool setPosition(const QString &screenName,
                               const QString &layoutKey, qreal x, qreal y);
  Q_INVOKABLE bool clearScreen(const QString &screenName);

private:
  void load();
  [[nodiscard]] bool save() const;
  [[nodiscard]] static bool validKey(const QString &value);

  QString m_storagePath;
  QVariantMap m_screens;
};

} // namespace QindaQt::Shell::DesktopSurface
