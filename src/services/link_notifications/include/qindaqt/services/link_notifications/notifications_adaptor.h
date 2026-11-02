// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_notifications/notification_feature.h>

#include <QtDBus/QDBusAbstractAdaptor>

namespace QindaQt::Link {

// org.qindaqt.Link1.Notifications on the /org/qindaqt/Link1 object (a
// QDBusAbstractAdaptor of the exporting Link1Service, which must register with
// ExportAdaptors). Kept separate so link1_service.cpp does not grow.
//
// AGENT-CONTRACT: Current/CurrentChanged carry titles and texts, like the
// freedesktop notifications bus does; they are session-bus, same-user only.
// docs/wiki/reference/link1-v1.md, "Notifications".
class NotificationsAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Link1.Notifications")
public:
    NotificationsAdaptor(NotificationFeature &feature, QObject *exported);

public Q_SLOTS:
    QString ListApps() const;
    void SetAppMuted(const QString &pkg, bool muted);
    // Returns "" once the request was sent, or an error code when it could not be
    // (not-found, bad-request, not-connected). The phone's own answer follows as
    // ReplyFinished. AGENT-NOTE: no delayed D-Bus reply: an adaptor's QDBusContext is
    // the exporting object's, which is Link1Service, not this class.
    QString Reply(const QString &device, const QString &key, const QString &action, const QString &text);
    QString Current() const;
    // Number of CurrentChanged signals sent; a client that sees a gap re-reads Current().
    uint Revision() const { return m_revision; }

Q_SIGNALS:
    // One entry per signal: the changed Current() item, or {device,key,removed:true}. `revision` counts signals.
    void CurrentChanged(uint revision, const QString &itemJson);
    // error is empty when the phone executed the action, else a protocol error code.
    void ReplyFinished(const QString &device, const QString &key, const QString &action, const QString &error);

private:
    NotificationFeature &m_feature;
    uint m_revision = 0;
};

} // namespace QindaQt::Link
