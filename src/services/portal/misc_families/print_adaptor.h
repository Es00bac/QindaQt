// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "misc_policy.h"
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QHash>
#include <functional>
#include <QDBusUnixFileDescriptor>
namespace QindaQt::Services::Portal {
class PrintAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Print")
public:
    PrintAdaptor(QObject &, RequestRegistry &, MiscUi &);
    ~PrintAdaptor() override;
public Q_SLOTS:
    quint32 PreparePrint(const QDBusObjectPath &, const QString &, const QString &, const QString &, const QVariantMap &, const QVariantMap &, const QVariantMap &, const QDBusMessage &, QVariantMap &);
    quint32 Print(const QDBusObjectPath &, const QString &, const QString &, const QString &, const QDBusUnixFileDescriptor &, const QVariantMap &, const QDBusMessage &, QVariantMap &);
private:
    struct Pending { bool prepare; QString app, owner; };
    struct Prepared { QString app, owner; QJsonObject configuration; qint64 expiry; };
    quint32 begin(bool, const QDBusObjectPath &, const QString &, const QString &, const QString &, const QVariantMap &, const QVariantMap &, const QVariantMap &, int, const QDBusMessage &, QVariantMap &);
    RequestRegistry &m_requests; MiscUi &m_ui;
    QHash<RequestToken, Pending> m_pending;
    QHash<quint32, Prepared> m_prepared;
};
}
