// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_notifications/app_settings.h>
#include <qindaqt/services/link_notifications/icon_cache.h>
#include <qindaqt/services/link_notifications/notification_presenter.h>
#include <qindaqt/services/link_notifications/phone_notification.h>
#include <qindaqt/services/link_transport/feature.h>

#include <QtCore/QHash>
#include <QtCore/QObject>

#include <functional>

namespace QindaQt::Link {

struct NotificationFeatureDeps {
    NotificationPresenter *presenter = nullptr;  // required, not owned
    IconCache *icons = nullptr;                  // required, not owned
    AppSettings *apps = nullptr;                 // required, not owned
    ReplyLauncher *reply = nullptr;              // optional: no launcher, no "Reply…" action
    std::function<Session *(const QString &deviceId)> sessionFor;  // READY session or null
    int iconTimeoutMs = 10'000;
    int actionGraceMs = 2'000;  // how long a close after an action button is not a user dismissal
};

// Desktop half of the `notify` capability: shows phone notifications through
// an injected presenter and sends dismissals and actions back.
//
// Ownership: dependencies are injected and must outlive the feature. Threading:
// the session thread only. Error behavior: invalid frames are dropped (a
// request gets error{bad-request}); nothing here throws.
//
// AGENT-GUARD: titles, texts and reply texts are personal content. Log frame
// types and sizes only. Closing a notification because the phone removed it
// (or any programmatic close) must never send notify.dismiss, or the two
// sides loop; only NotificationPresenter::userDismissed does.
// AGENT-CONTRACT: wire format is docs/wiki/reference/qindalink-protocol-v1.md,
// "Notifications"; the phone side is org/qindaqt/link/notify/NotificationFeature.java.
class NotificationFeature final : public QObject, public FeatureHandler {
    Q_OBJECT
public:
    explicit NotificationFeature(NotificationFeatureDeps deps, QObject *parent = nullptr);
    [[nodiscard]] QString ns() const override { return QStringLiteral("notify"); }
    void handle(Session &session, const Envelope &envelope) override;
    void sessionClosed(const QString &deviceId) override;

    // [{device,key,pkg,app,title,text,when,hasReply,actions}] for every tracked notification.
    [[nodiscard]] QJsonArray current() const;
    [[nodiscard]] QJsonArray listApps() const { return m_deps.apps->apps(); }
    void setAppMuted(const QString &pkg, bool muted);
    // Sends notify.action. `done` gets an empty string on success or an
    // error code (protocol error code, or "not-found"/"not-connected").
    void reply(const QString &deviceId, const QString &key, const QString &actionId, const QString &text,
               std::function<void(const QString &error)> done);

Q_SIGNALS:
    // One changed notification: its Current() entry, or {device,key,removed:true}. Never the full set.
    void entryChanged(const QJsonObject &item);

private:
    struct Entry {
        PhoneNotification n;
        quint32 handle = 0;
        quint64 gen = 0;
        bool actionTaken = false;
        quint64 actionToken = 0;
        bool quiet = false;
    };
    struct Waiter {
        QString device, key;
        quint64 gen;
    };

    void onPosted(Session &session, const PhoneNotification &n, bool quiet);
    void onSnapshot(Session &session, const QList<PhoneNotification> &items);
    void onRemoved(const QString &device, const QString &key);
    void onIcon(const QString &device, const QJsonObject &body);
    void flushIcon(const QString &device, const QString &hash);
    void show(const QString &device, const QString &key);
    [[nodiscard]] QJsonObject entryJson(const QString &device, const Entry &e) const;
    void erase(const QString &device, const QString &key, bool closeShown);
    void onUserDismissed(quint32 handle);
    void onActionInvoked(quint32 handle, const QString &actionKey);
    Entry *find(const QString &device, const QString &key);

    NotificationFeatureDeps m_deps;
    QHash<QString, QHash<QString, Entry>> m_active;  // device -> key -> entry
    QHash<QString, QStringList> m_order;              // device -> keys, oldest first
    QHash<quint32, QPair<QString, QString>> m_byHandle;
    QHash<QString, QList<Waiter>> m_waiting;          // "device/hash" -> entries waiting for the icon
    quint64 m_gen = 0;
};

} // namespace QindaQt::Link
