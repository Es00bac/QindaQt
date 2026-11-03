// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_service/lan_controller.h>
#include <qindaqt/services/link_service/lan_settings.h>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtNetwork/QNetworkInterface>

namespace QindaQt::Link {

bool isPrivateLanAddress(const QHostAddress &a)
{
    if (a.protocol() == QAbstractSocket::IPv4Protocol) {
        const quint32 v = a.toIPv4Address();
        return (v >> 24) == 10 || (v >> 20) == ((172u << 4) | 1u) || (v >> 16) == ((192u << 8) | 168u)
            || (v >> 22) == ((100u << 2) | 1u);   // 100.64.0.0/10
    }
    if (a.protocol() == QAbstractSocket::IPv6Protocol) {
        const Q_IPV6ADDR b = a.toIPv6Address();
        return (b[0] & 0xfe) == 0xfc;
    }
    return false;
}

QList<LanAddress> defaultLanAddresses()
{
    QList<LanAddress> out;
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        const auto flags = iface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) || !flags.testFlag(QNetworkInterface::IsRunning)
            || flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        const QString name = iface.name();
        bool skip = false;
        for (const char *prefix : {"docker", "veth", "br-", "virbr", "lxc", "podman"}) {
            skip |= name.startsWith(QLatin1String(prefix));
        }
        if (skip) continue;
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress a = entry.ip();
            if (a.isLoopback() || a.isNull() || a.isMulticast() || a.isLinkLocal() || !isPrivateLanAddress(a)) continue;
            out.append({a, name});
        }
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
    connect(&m_window, &QTimer::timeout, this, [this] { closeWindow(true); });
    if (m_options.publisher != nullptr) {
        connect(m_options.publisher, &ServicePublisher::stateChanged, this, &LanController::statusChanged);
    }
    m_enabled = LanSettings(m_options.configDir).lanEnabled();
    m_allowlist = LanSettings(m_options.configDir).lanInterfaces();
    connect(&m_hub, &Hub::lanPairingWindowConsumed, this, [this] { closeWindow(false); });
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
        closeWindow(true);
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

bool LanController::setInterfaces(const QStringList &names, QString *error)
{
    static const QRegularExpression valid(QStringLiteral("^[A-Za-z0-9_.:@-]{1,15}$"));
    QStringList clean;
    for (const QString &n : names) {
        if (!valid.match(n).hasMatch() || clean.size() >= 16) {
            if (error != nullptr) *error = QStringLiteral("invalid interface list");
            return false;
        }
        if (!clean.contains(n)) clean.append(n);
    }
    const bool ok = LanSettings(m_options.configDir).setLanInterfaces(clean, error);
    m_persistFailed = !ok;
    m_allowlist = clean;
    sync();
    return ok;
}

void LanController::sync()
{
    if (!m_enabled) return;
    QHash<QString, QString> desired;   // address text -> interface
    for (const LanAddress &a : m_options.addresses()) {
        if (m_allowlist.isEmpty() || m_allowlist.contains(a.iface)) desired.insert(a.address.toString(), a.iface);
    }
    for (auto it = m_bound.begin(); it != m_bound.end();) {
        if (!desired.contains(it.key())) {
            m_hub.unlisten(QHostAddress(it.key()), Origin::Lan);
            it = m_bound.erase(it);
        } else {
            ++it;
        }
    }
    m_failures.clear();
    for (auto it = desired.cbegin(); it != desired.cend(); ++it) {
        if (m_bound.contains(it.key())) {
            m_bound[it.key()].iface = it.value();   // the same address can move between interface names
            continue;
        }
        QString error;
        if (m_hub.listen(QHostAddress(it.key()), m_options.port, Origin::Lan, &error)) {
            m_bound.insert(it.key(), {m_hub.portOf(QHostAddress(it.key()), Origin::Lan), it.value()});
        } else {
            m_failures.insert(it.key(), error);
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
        QStringList ifaces;
        for (const Bound &b : std::as_const(m_bound)) {
            if (!ifaces.contains(b.iface)) ifaces.append(b.iface);
        }
        ifaces.sort();
        ServiceAdvert advert;
        advert.name = m_options.instanceName;
        advert.type = QStringLiteral("_qindalink._tcp");
        advert.port = m_bound.cbegin().value().port;
        advert.interfaces = ifaces;
        advert.txt = {{QStringLiteral("proto"), QStringLiteral("1")},
                      {QStringLiteral("id"), QString::fromLatin1(m_hub.identity().spkiDigest().toHex().left(16))},
                      {QStringLiteral("name"), m_options.instanceName}};
        // AGENT-NOTE: republishing on every resync would flap the record; only publish on change.
        if (!m_advertised || m_lastAdvertPort != advert.port || m_advertIfaces != ifaces) {
            p->publish(advert);
            m_lastAdvertPort = advert.port;
            m_advertIfaces = ifaces;
            m_advertised = true;
        }
    } else if (m_advertised) {
        p->withdraw();
        m_advertised = false;
        m_advertIfaces.clear();
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

void LanController::closeWindow(bool abort)
{
    // AGENT-GUARD: a single-shot QTimer is already inactive inside its own timeout, so track openness separately.
    const bool wasOpen = m_windowOpen;
    m_windowOpen = false;
    m_window.stop();
    m_hub.setPairingWindowOpen(false);
    // AGENT-GUARD: a window that ran out must not leave a LAN pairing running on its authority.
    if (abort) m_hub.abortLanPairings();
    if (wasOpen) Q_EMIT pairingWindowChanged(false, 0);
}

quint32 LanController::pairingWindowRemaining() const
{
    return m_window.isActive() ? quint32((m_window.remainingTime() + 999) / 1000) : 0;
}

QStringList LanController::boundInterfaces() const
{
    QStringList out;
    for (const Bound &b : m_bound) {
        if (!out.contains(b.iface)) out.append(b.iface);
    }
    out.sort();
    return out;
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
                  {QStringLiteral("interfaces"), QJsonArray::fromStringList(m_advertIfaces.isEmpty() ? boundInterfaces() : m_advertIfaces)},
                  {QStringLiteral("allowlist"), QJsonArray::fromStringList(m_allowlist)},
                  {QStringLiteral("failed"), failed},
                  {QStringLiteral("advertised"), m_advertised},
                  {QStringLiteral("publisher"), m_options.publisher != nullptr ? m_options.publisher->state() : QStringLiteral("none")},
                  {QStringLiteral("pairingWindow"), int(pairingWindowRemaining())},
                  {QStringLiteral("persisted"), !m_persistFailed}};
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

} // namespace QindaQt::Link
