// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/idle_inhibition.h>
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QSet>
namespace QindaQt::Services::Portal {
// Borrowed same-thread registry and native lease port outlive this adaptor.
// Inhibit replies are void; Request.Close ends the actual native lease.
class InhibitAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Inhibit")
public:
    InhibitAdaptor(QObject &host, RequestRegistry &, IdleInhibition &, QDBusConnection);
    ~InhibitAdaptor() override;
public Q_SLOTS:
    void Inhibit(const QDBusObjectPath &, const QString &app, const QString &window,
                 quint32 flags, const QVariantMap &, const QDBusMessage &);
    quint32 CreateMonitor(const QDBusObjectPath &, const QDBusObjectPath &,
                          const QString &, const QString &, const QDBusMessage &);
    void QueryEndResponse(const QDBusObjectPath &, const QDBusMessage &);
Q_SIGNALS:
    void StateChanged(const QDBusObjectPath &session, const QVariantMap &state);
private:
    RequestRegistry &m_requests; IdleInhibition &m_idle; QDBusConnection m_bus;
    QSet<RequestToken> m_tokens;
};
} // namespace QindaQt::Services::Portal
