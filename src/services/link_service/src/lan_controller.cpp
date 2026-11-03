// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_service/lan_controller.h>
#include <qindaqt/services/link_service/lan_settings.h>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtNetwork/QNetworkInterface>

namespace QindaQt::Link {

QList<QHostAddress> defaultLanAddresses()
{
    QList<QHostAddress> out;
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        const auto flags = iface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) || !flags.testFlag(QNetworkInterface::IsRunning)
            || flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        const QString name = iface.name();
        for (const char *skip : {"docker", "veth", "br-", "virbr", "lxc", "podman"}) {
            if (name.startsWith(QLatin1String(skip))) goto next;
        }
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress a = entry.ip();
            if (a.isLoopback() || a.isNull() || a.isMulticast() || a.isLinkLocal()) continue;
            if (a.protocol() != QAbstractSocket::IPv4Protocol && a.protocol() != QAbstractSocket::IPv6Protocol) continue;
            if (!out.contains(a)) out.append(a);
        }
    next:;
    }
    return out;
}

LanController::LanController(Hub &hub, LanOptions options, QObject *parent)
    : QObject(parent), m_hub(hub), m_options(std::move(options))
{
    if (!m_options.addresses) m_options.addresses = defaultLanAddresses;
    m_resync.setInterval(m_options.resyncMs);
    connect(&m_resync, &QTimer::timeout, this, &LanController::sync);
    m_window.setSingleShot(true);
    m_window.setInterval(m_options.pairingWindowMs);
    connect(&m_window, &QTimer::timeout, this, &LanController::closeWindow);
    if (m_options.publisher != nullptr) {
        connect(m_options.publisher, &ServicePublisher::stateChanged, this, &LanController::statusChanged);
    }
    m_enabled = LanSettings(m_options.configDir).lanEnabled();
    if (m_enabled) {
        m_resync.start();
        sync();
    }
}

LanController::~LanController()
{
    if (m_advertised && m_options.publisher != nullptr) m_options.publisher->withdraw();
}

bool LanController::setEnabled(bool enabled)
{
    QString error;
    m_persistFailed = !LanSettings(m_options.configDir).setLanEnabled(enabled, &error);
    if (enabled == m_enabled) {
        Q_EMIT statusChanged();
        return !m_persistFailed;
    }
    m_enabled = enabled;
    if (enabled) {
        m_resync.start();
        sync();
    } else {
        m_resync.stop();
        closeWindow();
        for (auto it = m_bound.cbegin(); it != m_bound.cend(); ++it) {
            m_hub.unlisten(QHostAddress(it.key()), Origin::Lan);
        }
        m_bound.clear();
        m_failures.clear();
        m_hub.dropSessions(Origin::Lan);
        applyAdvert();
    }
    Q_EMIT statusChanged();
    return !m_persistFailed;
}

void LanController::sync()
{
    if (!m_enabled) return;
    QSet<QString> desired;
    for (const QHostAddress &a : m_options.addresses()) desired.insert(a.toString());
    for (auto it = m_bound.begin(); it != m_bound.end();) {
        if (!desired.contains(it.key())) {
            m_hub.unlisten(QHostAddress(it.key()), Origin::Lan);
            it = m_bound.erase(it);
        } else {
            ++it;
        }
    }
    m_failures.clear();
    for (const QString &text : std::as_const(desired)) {
        if (m_bound.contains(text)) continue;
        QString error;
        if (m_hub.listen(QHostAddress(text), m_options.port, Origin::Lan, &error)) {
            m_bound.insert(text, m_hub.portOf(QHostAddress(text), Origin::Lan));
        } else {
            m_failures.insert(text, error);
        }
    }
    applyAdvert();
    Q_EMIT statusChanged();
}

void LanController::applyAdvert()
{
    ServicePublisher *p = m_options.publisher;
    if (p == nullptr) return;
    if (m_enabled && !m_bound.isEmpty()) {
        ServiceAdvert advert;
        advert.name = m_options.instanceName;
        advert.type = QStringLiteral("_qindalink._tcp");
        advert.port = m_bound.cbegin().value();
        advert.txt = {{QStringLiteral("proto"), QStringLiteral("1")},
                      {QStringLiteral("id"), QString::fromLatin1(m_hub.identity().spkiDigest().toHex().left(16))},
                      {QStringLiteral("name"), m_options.instanceName}};
        // AGENT-NOTE: republishing on every resync would flap the record; only publish on change.
        if (!m_advertised || m_lastAdvertPort != advert.port) {
            p->publish(advert);
            m_lastAdvertPort = advert.port;
            m_advertised = true;
        }
    } else if (m_advertised) {
        p->withdraw();
        m_advertised = false;
    }
}

quint32 LanController::openPairingWindow()
{
    if (!m_enabled) return 0;
    m_window.start();
    m_windowOpen = true;
    m_hub.setPairingWindowOpen(true);
    const quint32 seconds = quint32(m_options.pairingWindowMs / 1000);
    Q_EMIT pairingWindowChanged(true, seconds);
    return seconds;
}

void LanController::closeWindow()
{
    // AGENT-GUARD: a single-shot QTimer is already inactive inside its own timeout, so track openness separately.
    const bool wasOpen = m_windowOpen;
    m_windowOpen = false;
    m_window.stop();
    m_hub.setPairingWindowOpen(false);
    if (wasOpen) Q_EMIT pairingWindowChanged(false, 0);
}

quint32 LanController::pairingWindowRemaining() const
{
    return m_window.isActive() ? quint32((m_window.remainingTime() + 999) / 1000) : 0;
}

QString LanController::statusJson() const
{
    QString state = QStringLiteral("off");
    if (m_enabled) {
        state = !m_bound.isEmpty() ? QStringLiteral("listening")
            : m_failures.isEmpty() ? QStringLiteral("waiting") : QStringLiteral("error");
    }
    QJsonArray failed;
    for (const QString &reason : m_failures) failed.append(reason);
    QJsonObject o{{QStringLiteral("enabled"), m_enabled},
                  {QStringLiteral("state"), state},
                  {QStringLiteral("listeners"), int(m_bound.size())},
                  {QStringLiteral("failed"), failed},
                  {QStringLiteral("advertised"), m_advertised},
                  {QStringLiteral("publisher"), m_options.publisher != nullptr ? m_options.publisher->state() : QStringLiteral("none")},
                  {QStringLiteral("pairingWindow"), int(pairingWindowRemaining())},
                  {QStringLiteral("persisted"), !m_persistFailed}};
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

} // namespace QindaQt::Link
