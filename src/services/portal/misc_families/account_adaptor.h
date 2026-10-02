// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "misc_policy.h"
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QHash>
#include <functional>
namespace QindaQt::Services::Portal {
// Injected data provider belongs to native composition, never to frontend options.
class AccountAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Account")
public:
    using Provider = std::function<AccountInformation()>;
    AccountAdaptor(QObject &, RequestRegistry &, MiscUi &, Provider = localAccountInformation);
    ~AccountAdaptor() override;
public Q_SLOTS:
    quint32 GetUserInformation(const QDBusObjectPath &, const QString &, const QString &, const QVariantMap &, const QDBusMessage &, QVariantMap &);
private:
    RequestRegistry &m_requests; MiscUi &m_ui;
    Provider m_provider; QHash<RequestToken, AccountInformation> m_pending;
};
}
