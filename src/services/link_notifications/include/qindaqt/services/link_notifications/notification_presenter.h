// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPair>
#include <QtCore/QString>

namespace QindaQt::Link {

// What the desktop shows for one phone notification. Strings are already
// bounded and sanitized by the feature; title/body are personal content.
struct PresentedNotification {
    QString appName;                      // "<label> (phone)"
    QString summary;
    QString body;
    QString iconPath;                     // absolute PNG path or empty
    QList<QPair<QString, QString>> actions;  // (key, label)
    bool quiet = false;                   // restored from a snapshot: no sound
};

// Shows and closes desktop notifications for the notify feature.
// Production is FdoPhoneNotificationPresenter (org.freedesktop.Notifications
// on an injected bus); tests and the private-bus live check use fakes, so no
// test ever talks to the user's real notification server.
//
// AGENT-CONTRACT: present() returns a nonzero handle immediately, even when
// the server answers later. A nonzero `replaces` updates that notification in
// place; if it is gone a new one is shown and the same handle is kept.
// close() is silent: it never causes userDismissed(). userDismissed(handle)
// is emitted only when the user closed it (freedesktop reason 2). Handles are
// never reused during a run.
class NotificationPresenter : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual quint32 present(const PresentedNotification &n, quint32 replaces) = 0;
    virtual void close(quint32 handle) = 0;

Q_SIGNALS:
    void userDismissed(quint32 handle);
    void actionInvoked(quint32 handle, const QString &key);
};

// Launches the desktop UI for typing a reply. The QindaQt notification server
// has no inline reply, so reply actions appear as "Reply…" only when this
// reports available().
class ReplyLauncher {
public:
    virtual ~ReplyLauncher() = default;
    [[nodiscard]] virtual bool available() const = 0;
    virtual void launch(const QString &deviceId, const QString &key, const QString &actionId) = 0;
};

} // namespace QindaQt::Link
