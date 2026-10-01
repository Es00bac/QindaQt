// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/access_consent.h>
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QHash>
namespace QindaQt::Services::Portal {
// Configure on the public backend host before its ExportAdaptors registration.
// Borrowed registry/consent must outlive this same-thread adaptor. Destruction
// cancels its own requests only; appearance and Secret remain independent.
class AccessAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Access")
public:
    AccessAdaptor(QObject &host, RequestRegistry &, AccessConsent &);
    ~AccessAdaptor() override;
public Q_SLOTS:
    quint32 AccessDialog(const QDBusObjectPath &handle, const QString &appId,
        const QString &parentWindow, const QString &title, const QString &subtitle,
        const QString &body, const QVariantMap &options, const QDBusMessage &call,
        QVariantMap &results);
private:
    RequestRegistry &m_requests;
    AccessConsent &m_consent;
    QHash<RequestToken, AccessQuestion> m_questions;
};
} // namespace QindaQt::Services::Portal
