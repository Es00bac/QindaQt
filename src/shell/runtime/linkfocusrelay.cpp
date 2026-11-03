// SPDX-License-Identifier: GPL-3.0-or-later

#include "linkfocusrelay.h"

#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QJsonDocument>
#include <QJsonObject>

namespace QindaQt::Shell {
namespace {
constexpr auto kService = "org.qindaqt.Link1";
constexpr auto kNone = "{}";
}

LinkFocusRelay::LinkFocusRelay(QDBusConnection bus, Provider provider, QObject *parent)
    : QObject(parent)
    , m_bus(std::move(bus))
    , m_provider(std::move(provider))
    , m_watcher(QString::fromLatin1(kService), m_bus, QDBusServiceWatcher::WatchForOwnerChange, this)
{
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, [this] { refresh(true); });
    connect(&m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &, const QString &, const QString &newOwner) {
                m_serviceUp = !newOwner.isEmpty();
                m_lastAccepted.clear(); // a new service instance knows nothing
                m_inFlight.clear();
                if (m_serviceUp) {
                    refresh();
                }
            });
}

void LinkFocusRelay::start()
{
    if (m_bus.interface() != nullptr) {
        m_serviceUp = m_bus.interface()->isServiceRegistered(QString::fromLatin1(kService));
    }
    m_timer.start();
    refresh();
}

QString LinkFocusRelay::factJson(
    const std::optional<Compositor::ShellWindowIdentitySnapshot> &identity,
    const WindowFacts &facts)
{
    if (!identity || !identity->available() || !identity->activeWindow) {
        return QString::fromLatin1(kNone);
    }
    const QString windowId = identity->activeWindow->windowId;
    QString appId;
    QString title;
    if (windowId.isEmpty() || !facts || !facts(windowId, &appId, &title)) {
        return QString::fromLatin1(kNone);
    }
    return QString::fromUtf8(QJsonDocument(QJsonObject{
                                               {QStringLiteral("uuid"), windowId},
                                               {QStringLiteral("appId"), appId},
                                               {QStringLiteral("title"), title},
                                               {QStringLiteral("revision"), double(identity->revision)}})
                                 .toJson(QJsonDocument::Compact));
}

void LinkFocusRelay::refresh(bool heartbeat)
{
    if (!m_serviceUp || !m_provider) {
        return;
    }
    const QString json = m_provider();
    if ((!heartbeat && json == m_lastAccepted) || json == m_inFlight) {
        return;
    }
    publish(json);
}

void LinkFocusRelay::publish(const QString &json)
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(kService), QStringLiteral("/org/qindaqt/Link1"),
        QStringLiteral("org.qindaqt.Link1.Focus"), QStringLiteral("Publish"));
    call.setAutoStartService(false);
    call << json;
    m_inFlight = json;
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 2000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, json](QDBusPendingCallWatcher *done) {
        done->deleteLater();
        if (m_inFlight == json) {
            m_inFlight.clear();
        }
        const QDBusPendingReply<> reply = *done;
        if (!reply.isError()) {
            m_lastAccepted = json;
            return;
        }
        // The next refresh retries. Denied means this shell is not the service's trusted shell.
        if (reply.error().name() == QLatin1String("org.qindaqt.Link1.Error.Denied") && !m_warnedDenied) {
            m_warnedDenied = true;
            qWarning("QindaLink focus relay: the link service denied Focus.Publish; secret typing stays unavailable");
        }
    });
}

} // namespace QindaQt::Shell
