// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "misc_policy.h"
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QHash>
#include <functional>
namespace QindaQt::Services::Portal {
class LauncherAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.DynamicLauncher")
    Q_PROPERTY(uint version READ version CONSTANT)
    Q_PROPERTY(uint SupportedLauncherTypes READ SupportedLauncherTypes CONSTANT)
public:
    LauncherAdaptor(QObject &, RequestRegistry &, MiscUi &);
    ~LauncherAdaptor() override;
    uint version() const { return 1; }
    uint SupportedLauncherTypes() const { return 3; }
public Q_SLOTS:
    quint32 PrepareInstall(const QDBusObjectPath &, const QString &, const QString &, const QString &, const QDBusVariant &, const QVariantMap &, const QDBusMessage &, QVariantMap &);
    quint32 RequestInstallToken(const QString &, const QVariantMap &, const QDBusMessage &);
private:
    RequestRegistry &m_requests; MiscUi &m_ui;
    QHash<RequestToken, QJsonObject> m_pending;
};
}
