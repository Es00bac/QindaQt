// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_notifications/notification_feature.h>
#include <qindaqt/services/link_transport/caller_authority.h>

#include <QtDBus/QDBusAbstractAdaptor>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>

namespace QindaQt::Link {

// org.qindaqt.Link1.Notifications on the /org/qindaqt/Link1 object (a
// QDBusAbstractAdaptor of the exporting Link1Service, which must register with
// ExportAdaptors). Kept separate so link1_service.cpp does not grow.
//
// AGENT-CONTRACT (ADR-0438): ListApps, SetAppMuted, Reply and Current need a trusted
// caller (CallerAuthority) and fail with Error.Denied otherwise. CurrentChanged is
// broadcast to every bus client and therefore carries identifiers only
// ({device,key[,removed]} plus the revision); trusted clients read content via Current().
// docs/wiki/reference/link1-v1.md, "Notifications".
class NotificationsAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Link1.Notifications")
public:
    // `authority` is required: a null one denies everything.
    NotificationsAdaptor(NotificationFeature &feature, QObject *exported, CallerAuthority *authority, QDBusConnection bus);

public Q_SLOTS:
    QString ListApps(const QDBusMessage &msg);
    void SetAppMuted(const QString &pkg, bool muted, const QDBusMessage &msg);
    // Returns "" once the request was sent, or an error code when it could not be
    // (not-found, bad-request, not-connected). The phone's own answer follows as
    // ReplyFinished. AGENT-NOTE: no delayed D-Bus reply: an adaptor's QDBusContext is
    // the exporting object's, which is Link1Service, not this class.
    QString Reply(const QString &device, const QString &key, const QString &action, const QString &text,
                  const QDBusMessage &msg);
    QString Current(const QDBusMessage &msg);
    // Number of CurrentChanged signals sent; a client that sees a gap re-reads Current().
    uint Revision() const { return m_revision; }

Q_SIGNALS:
    // One entry per signal: {device,key} of the changed item, plus removed:true when it went away.
    // `revision` counts signals. No notification text is ever broadcast.
    void CurrentChanged(uint revision, const QString &itemJson);
    // error is empty when the phone executed the action, else a protocol error code.
    void ReplyFinished(const QString &device, const QString &key, const QString &action, const QString &error);

private:
    [[nodiscard]] bool allowed(const QDBusMessage &msg);

    NotificationFeature &m_feature;
    CallerAuthority *m_authority;
    QDBusConnection m_bus;
    uint m_revision = 0;
};

} // namespace QindaQt::Link
