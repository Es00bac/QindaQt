// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/apps/settings_portal_permissions/portal_permissions_model.h>
#include <QtQml/qqmlregistration.h>
namespace QindaQt::Apps::SettingsPortalPermissions {
class PortalPermissionsRouteComposition final : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(QObject *model READ model CONSTANT)
public:
  explicit PortalPermissionsRouteComposition(QObject *parent = nullptr)
      : QObject(parent), m_model(QDBusConnection::sessionBus()) {}
  QObject *model() { return &m_model; }
private:
  PortalPermissionsModel m_model;
};
}
