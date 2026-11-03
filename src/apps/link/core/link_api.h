// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <functional>

namespace QindaQt::LinkApp {

// Completion of one asynchronous Link1 call: `result` is the service's JSON text and `error` is
// empty on success, else a lower-case protocol code (denied, unsupported, not-connected,
// bad-request, bad-reply, timeout, internal) or "unavailable" when the service is not running.
using Reply = std::function<void(const QString &result, const QString &error)>;

// The seam between the QindaLink app's view models and the Link1 service
// (docs/wiki/reference/link1-v1.md). DbusLinkApi talks to the session bus; FakeLinkApi serves
// scripted data for tests and `--fake` renders, so no view model ever needs a real phone.
//
// Ownership: the owner keeps the object alive for the application's lifetime. Threading: the
// GUI thread only; every Reply runs on it, later than the call (never re-entrantly).
// AGENT-GUARD: nothing behind this interface may log message bodies, addresses, names, snippets,
// notification text or reply text; log method names, ids and lengths only.
class LinkApi : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;

    [[nodiscard]] virtual bool available() const = 0;

    virtual void devices(Reply done) = 0;
    // Link1.PendingPairings, added by the caller-policy slice. An older service answers "unsupported".
    virtual void pendingPairings(Reply done) = 0;
    virtual void confirmPairing(const QString &id, bool accept) = 0;
    virtual void unpair(const QString &id) = 0;

    virtual void listThreads(const QString &device, int limit, qint64 before, qint64 beforeId, Reply done) = 0;
    virtual void listMessages(const QString &device, qint64 thread, int limit, qint64 before,
                              const QString &beforeKind, qint64 beforeId, Reply done) = 0;
    virtual void getPart(const QString &device, qint64 part, int maxEdge, Reply done) = 0;
    virtual void send(const QString &device, const QStringList &addrs, const QString &text, Reply done) = 0;
    virtual void markRead(const QString &device, qint64 thread, Reply done) = 0;

    virtual void listApps(Reply done) = 0;
    virtual void setAppMuted(const QString &pkg, bool muted) = 0;
    virtual void currentNotifications(Reply done) = 0;
    virtual void reply(const QString &device, const QString &key, const QString &action, const QString &text,
                       Reply done) = 0;

    virtual void adbDevices(Reply done) = 0;
    virtual void setupPhone(const QString &serial, Reply done) = 0;
    virtual void setupStatus(const QString &serial, Reply done) = 0;

Q_SIGNALS:
    void availableChanged();
    void devicesChanged(const QString &json);
    void pairingRequested(const QString &id, const QString &name, const QString &model, const QString &code);
    void threadsChanged(const QString &device, const QString &json);
    void sendStatus(const QString &device, qlonglong ref, const QString &state, const QString &code,
                    const QString &detail);
    void replyFinished(const QString &device, const QString &key, const QString &action, const QString &error);
};

} // namespace QindaQt::LinkApp
