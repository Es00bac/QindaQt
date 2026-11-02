// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_notifications/notification_feature.h>

#include <QtDBus/QDBusAbstractAdaptor>
#include <QtDBus/QDBusContext>

namespace QindaQt::Link {

// org.qindaqt.Link1.Notifications on the /org/qindaqt/Link1 object (a
// QDBusAbstractAdaptor of the exporting Link1Service, which must register with
// ExportAdaptors). Kept separate so link1_service.cpp does not grow.
//
// AGENT-CONTRACT: Current/CurrentChanged carry titles and texts, like the
// freedesktop notifications bus does; they are session-bus, same-user only.
// docs/wiki/reference/link1-v1.md, "Notifications".
class NotificationsAdaptor final : public QDBusAbstractAdaptor, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Link1.Notifications")
public:
    NotificationsAdaptor(NotificationFeature &feature, QObject *exported);

public Q_SLOTS:
    QString ListApps() const;
    void SetAppMuted(const QString &pkg, bool muted);
    // Returns "" on success or an error code; replies when the phone answers.
    QString Reply(const QString &device, const QString &key, const QString &action, const QString &text);
    QString Current() const;

Q_SIGNALS:
    void CurrentChanged(const QString &json);

private:
    NotificationFeature &m_feature;
};

} // namespace QindaQt::Link
