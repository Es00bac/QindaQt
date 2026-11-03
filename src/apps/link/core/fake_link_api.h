// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "link_api.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QMap>

namespace QindaQt::LinkApp {

// Scripted LinkApi for tests and offscreen `--fake` renders. Every name, number and message in
// it is invented; it never touches a bus, a phone or the user's data.
//
// Replies are delivered from the event loop (a zero-length timer), like the real client.
// AGENT-NOTE: tests reach in through the public members to script a phone and to read what the
// view models asked for. `calls` records method names only, never arguments with content.
class FakeLinkApi final : public LinkApi {
    Q_OBJECT
public:
    enum class Scenario { Populated, Empty, Unavailable, Error };
    explicit FakeLinkApi(Scenario scenario = Scenario::Populated, QObject *parent = nullptr);

    [[nodiscard]] static Scenario scenarioFromName(const QString &name, bool *ok = nullptr);
    // The `qindaqt-link-mirror --list-json` output the Populated scenario's phone would give.
    [[nodiscard]] static QByteArray inventoryJson();

    [[nodiscard]] bool available() const override { return m_available; }
    void setAvailable(bool available);

    void devices(Reply done) override;
    void pendingPairings(Reply done) override;
    void confirmPairing(const QString &id, bool accept) override;
    void unpair(const QString &id) override;
    void listThreads(const QString &device, int limit, qint64 before, qint64 beforeId, Reply done) override;
    void listMessages(const QString &device, qint64 thread, int limit, qint64 before, const QString &beforeKind,
                      qint64 beforeId, Reply done) override;
    void getPart(const QString &device, qint64 part, int maxEdge, Reply done) override;
    void send(const QString &device, const QStringList &addrs, const QString &text, Reply done) override;
    void markRead(const QString &device, qint64 thread, Reply done) override;
    void listApps(Reply done) override;
    void setAppMuted(const QString &pkg, bool muted) override;
    void currentNotifications(Reply done) override;
    void reply(const QString &device, const QString &key, const QString &action, const QString &text,
               Reply done) override;
    void adbDevices(Reply done) override;
    void setupPhone(const QString &serial, Reply done) override;
    void setupStatus(const QString &serial, Reply done) override;

    // Scripted state, public on purpose (see AGENT-NOTE).
    QString devicesJson;
    bool pendingSupported = true;
    QJsonArray pending;                           // {id,name,model,code}
    QList<QJsonObject> threads;                   // newest first
    QMap<qint64, QList<QJsonObject>> messages;    // per thread, newest first
    QJsonArray apps;                              // {pkg,label,muted}
    QJsonArray currentJson;
    QJsonArray adb;                               // {serial,appInstalled,tunneled}
    QString setupReportJson;
    QString statusReportJson;
    QString errorForAll;                          // when set, every data call fails with this code
    QString sendError;
    qint64 nextRef = 100;

    QStringList calls;                            // method names, in order
    QList<QStringList> sentTo;                    // recipients of each Send (addresses only)
    QStringList sentTexts;                        // test-only: the text handed to Send
    QMap<QString, bool> confirmations;            // device id -> last accept flag
    QStringList unpaired;
    QMap<QString, bool> muted;
    QStringList replies;                          // "device|key|action" of each Reply

private:
    void later(std::function<void()> work);
    [[nodiscard]] bool fails(const Reply &done);
    void populate();

    bool m_available = true;
};

} // namespace QindaQt::LinkApp
