// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/firewall_client/firewall_transport.h>

#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace QindaQt::Shell::FirewallApplet::TestSupport
{

// Deterministic fake for org.qindaqt.Firewall1 (QindaFirewall
// docs/monitor.md). Every request answers synchronously from the fields
// below; a test drives availability, property values, and the two
// asynchronous signals (Changed/AlertRaised/Responded via the helpers) and
// inspects the three fire-and-forget calls for which id they targeted.
class FakeFirewallTransport final : public Firewall::Client::FirewallTransport
{
    Q_OBJECT
public:
    [[nodiscard]] bool start(QString *error = nullptr) override
    {
        if (error != nullptr) {
            error->clear();
        }
        m_running = true;
        return true;
    }
    void stop() override { m_running = false; }

    void requestSnapshot(const quint64 token) override
    {
        ++snapshotRequests;
        if (!available) {
            Q_EMIT requestFailed(token, QStringLiteral("firewall service is unavailable"));
            return;
        }
        Q_EMIT snapshotReceived(token, state, headlineJson, alertCount, topAlertJson);
    }
    void requestAlerts(const quint64 token) override
    {
        Q_EMIT alertsReceived(token, alertsJson);
    }
    void requestAlert(const quint64 token, const QString &id) override
    {
        Q_EMIT alertReceived(token, alertJsonById.value(id));
    }
    void requestDismiss(const QString &id) override { dismissed.append(id); }
    void requestConfirmMine(const QString &id) override { confirmedMine.append(id); }
    void requestOpenWindow(const QString &id) override { openedWindow.append(id); }

    // Test control surface. `announceAvailable` is the only path that
    // emits `serviceAvailableChanged`; setters below just update the data
    // the next `requestSnapshot` (or an explicit `emitChanged`) exposes.
    void announceAvailable(const bool value)
    {
        if (value == available) {
            return;
        }
        available = value;
        Q_EMIT serviceAvailableChanged(value);
    }
    void setProperties(const QString &newState, const QByteArray &newHeadlineJson,
                       const quint32 newAlertCount, const QByteArray &newTopAlertJson)
    {
        state = newState;
        headlineJson = newHeadlineJson;
        alertCount = newAlertCount;
        topAlertJson = newTopAlertJson;
    }
    void emitChanged() { Q_EMIT changed(); }
    void emitAlertRaised(const QByteArray &json) { Q_EMIT alertRaised(json); }
    void emitResponded(const QByteArray &json) { Q_EMIT responded(json); }

    [[nodiscard]] bool running() const noexcept { return m_running; }

    bool available = false;
    QString state;
    QByteArray headlineJson;
    quint32 alertCount = 0;
    QByteArray topAlertJson;
    QByteArray alertsJson;
    QHash<QString, QByteArray> alertJsonById;

    int snapshotRequests = 0;
    QStringList dismissed;
    QStringList confirmedMine;
    QStringList openedWindow;

private:
    bool m_running = false;
};

inline QByteArray headlineJson(const QString &level, const QString &title, const QString &text)
{
    QJsonObject object;
    object.insert(QStringLiteral("level"), level);
    object.insert(QStringLiteral("title"), title);
    object.insert(QStringLiteral("text"), text);
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

inline QByteArray alertJson(const QString &id, const QString &severity, const QString &title,
                           const QString &summary, const QString &meaning,
                           const QString &advice, const bool canConfirm,
                           const bool canBlock = true)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("kind"), QStringLiteral("password-guessing"));
    object.insert(QStringLiteral("severity"), severity);
    object.insert(QStringLiteral("severityLabel"), severity);
    object.insert(QStringLiteral("title"), title);
    object.insert(QStringLiteral("summary"), summary);
    object.insert(QStringLiteral("meaning"), meaning);
    object.insert(QStringLiteral("advice"), advice);
    object.insert(QStringLiteral("canBlock"), canBlock);
    object.insert(QStringLiteral("canConfirm"), canConfirm);
    object.insert(QStringLiteral("address"), QStringLiteral("203.0.113.9"));
    object.insert(QStringLiteral("place"), QStringLiteral("on the internet"));
    object.insert(QStringLiteral("count"), 12);
    object.insert(QStringLiteral("ongoing"), true);
    object.insert(QStringLiteral("remote"), true);
    object.insert(QStringLiteral("evidence"), QJsonArray());
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

} // namespace QindaQt::Shell::FirewallApplet::TestSupport
