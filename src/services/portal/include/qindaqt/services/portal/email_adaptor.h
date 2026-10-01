// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/request_registry.h>
#include <qindaqt/services/application_uri/application_uri_opener.h>
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QSet>
namespace QindaQt::Services::Portal {
// Same-thread standard Email backend. Borrowed registry/opener and readonly,
// non-reentrant native-session admission captures outlive this adaptor. The
// backend opens one draft; it never sends mail. Close before native handoff
// cancels it; a handed-off application owns its independent draft lifetime.
class EmailAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Email")
public:
    EmailAdaptor(QObject &host, RequestRegistry &,
        QindaQt::Services::ApplicationUri::ApplicationUriOpener &, std::function<bool()> admission);
    ~EmailAdaptor() override;
public Q_SLOTS:
    quint32 ComposeEmail(const QDBusObjectPath &, const QString &app, const QString &parent,
        const QVariantMap &, const QDBusMessage &, QVariantMap &results);
private:
    RequestRegistry &m_requests;
    QindaQt::Services::ApplicationUri::ApplicationUriOpener &m_opener;
    std::function<bool()> m_admission;
    QSet<RequestToken> m_tokens;
};
} // namespace QindaQt::Services::Portal
