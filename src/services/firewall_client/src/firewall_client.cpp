// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/firewall_client/firewall_client.h>

#include <qindaqt/services/firewall_client/firewall_transport.h>
#include <qindaqt/services/firewall_protocol/firewall_codec.h>

#include <utility>

namespace QindaQt::Firewall::Client {

FirewallClient::FirewallClient(FirewallTransport &transport, QObject *parent)
    : QObject(parent)
    , m_transport(transport)
{
    connect(&m_transport, &FirewallTransport::serviceAvailableChanged, this,
            &FirewallClient::handleServiceAvailableChanged);
    connect(&m_transport, &FirewallTransport::changed, this,
            &FirewallClient::handleTransportChanged);
    connect(&m_transport, &FirewallTransport::alertRaised, this,
            &FirewallClient::handleAlertRaised);
    connect(&m_transport, &FirewallTransport::responded, this,
            &FirewallClient::handleResponded);
    connect(&m_transport, &FirewallTransport::snapshotReceived, this,
            &FirewallClient::handleSnapshotReceived);
    connect(&m_transport, &FirewallTransport::alertsReceived, this,
            &FirewallClient::handleAlertsReceived);
    connect(&m_transport, &FirewallTransport::alertReceived, this,
            &FirewallClient::handleAlertReceived);
    connect(&m_transport, &FirewallTransport::requestFailed, this,
            &FirewallClient::handleRequestFailed);
}

FirewallClient::~FirewallClient() { stop(); }

bool FirewallClient::start(QString *error)
{
    if (m_started) {
        if (error != nullptr) {
            error->clear();
        }
        return true;
    }
    QString transportError;
    if (!m_transport.start(&transportError)) {
        if (error != nullptr) {
            *error = transportError;
        }
        return false;
    }
    m_started = true;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

void FirewallClient::stop()
{
    if (!m_started) {
        return;
    }
    m_started = false;
    const bool previousAvailable = m_serviceAvailable;
    const QString previousState = m_state;
    m_transport.stop();
    m_serviceAvailable = false;
    m_state.clear();
    m_headline = Headline{};
    m_alertCount = 0;
    m_topAlert.reset();
    m_snapshotToken.reset();
    m_snapshotDirty = false;
    for (auto &callback : m_alertsCallbacks) {
        if (callback) {
            callback({});
        }
    }
    m_alertsCallbacks.clear();
    for (auto &callback : m_alertCallbacks) {
        if (callback) {
            callback(std::nullopt);
        }
    }
    m_alertCallbacks.clear();
    publishStateIfChanged(previousAvailable, previousState);
    Q_EMIT changed();
}

void FirewallClient::refresh()
{
    if (!m_started || !m_serviceAvailable) {
        return;
    }
    if (m_snapshotToken.has_value()) {
        m_snapshotDirty = true;
        return;
    }
    requestSnapshotNow();
}

void FirewallClient::dismiss(const QString &id)
{
    if (!m_started) {
        return;
    }
    m_transport.requestDismiss(id);
}

void FirewallClient::confirmMine(const QString &id)
{
    if (!m_started) {
        return;
    }
    m_transport.requestConfirmMine(id);
}

void FirewallClient::openWindow(const QString &id)
{
    if (!m_started) {
        return;
    }
    // AGENT-NOTE: forwarded even while serviceAvailable() is false. This is
    // the Unavailable state's own recovery action ("Open QindaFirewall to
    // turn it on"); a transport that cannot reach or activate the name
    // simply fails quietly, exactly like the other two fire-and-forget calls.
    m_transport.requestOpenWindow(id);
}

void FirewallClient::requestAlerts(AlertsCallback callback)
{
    if (!m_started || !m_serviceAvailable) {
        if (callback) {
            callback({});
        }
        return;
    }
    const quint64 token = nextToken();
    m_alertsCallbacks.insert(token, std::move(callback));
    m_transport.requestAlerts(token);
}

void FirewallClient::requestAlert(const QString &id, AlertCallback callback)
{
    if (!m_started || !m_serviceAvailable) {
        if (callback) {
            callback(std::nullopt);
        }
        return;
    }
    const quint64 token = nextToken();
    m_alertCallbacks.insert(token, std::move(callback));
    m_transport.requestAlert(token, id);
}

void FirewallClient::handleServiceAvailableChanged(const bool available)
{
    if (!m_started || available == m_serviceAvailable) {
        return;
    }
    const bool previousAvailable = m_serviceAvailable;
    const QString previousState = m_state;
    m_serviceAvailable = available;
    m_snapshotToken.reset();
    m_snapshotDirty = false;
    if (!available) {
        // AGENT-GUARD: owner loss retires every published fact. Showing the
        // retired owner's last headline/alert as current would claim a
        // security posture QindaFirewall no longer vouches for.
        m_state.clear();
        m_headline = Headline{};
        m_alertCount = 0;
        m_topAlert.reset();
    }
    publishStateIfChanged(previousAvailable, previousState);
    Q_EMIT changed();
    if (available) {
        requestSnapshotNow();
    }
}

void FirewallClient::handleTransportChanged()
{
    if (!m_started || !m_serviceAvailable) {
        return;
    }
    // `Changed()` is an invalidation hint only ("any property may have
    // changed"); refetch rather than guess, exactly like Network1's
    // snapshotInvalidated. handleSnapshotReceived decides whether anything
    // actually differs before publishing.
    refresh();
}

void FirewallClient::handleAlertRaised(const QByteArray &alertJson)
{
    if (!m_started) {
        return;
    }
    Q_EMIT alertRaised(QString::fromUtf8(alertJson));
    refresh();
}

void FirewallClient::handleResponded(const QByteArray &responseJson)
{
    if (!m_started) {
        return;
    }
    Q_EMIT responded(QString::fromUtf8(responseJson));
    // A response (for example an automatic block) can itself change what
    // AlertCount/TopAlert report, so refresh exactly as for AlertRaised.
    refresh();
}

void FirewallClient::handleSnapshotReceived(
    const quint64 token, const QString &state, const QByteArray &headlineJson,
    const quint32 alertCount, const QByteArray &topAlertJson)
{
    if (!m_started || !m_snapshotToken.has_value() || *m_snapshotToken != token) {
        return;
    }
    m_snapshotToken.reset();
    const bool dirty = m_snapshotDirty;
    m_snapshotDirty = false;

    const bool previousAvailable = m_serviceAvailable;
    const QString previousState = m_state;
    const Headline previousHeadline = m_headline;
    const quint32 previousAlertCount = m_alertCount;
    const std::optional<Alert> previousTopAlert = m_topAlert;

    m_state = state;
    Headline headline;
    // A failed decode leaves `headline` default-constructed (level
    // Unknown), which is exactly the right fallback; the return value is
    // intentionally unused here.
    static_cast<void>(decodeHeadline(headlineJson, &headline));
    m_headline = headline;
    m_alertCount = alertCount;
    Alert alert;
    m_topAlert = decodeAlert(topAlertJson, &alert) ? std::optional<Alert>(std::move(alert))
                                                   : std::nullopt;

    publishStateIfChanged(previousAvailable, previousState);
    const bool unchanged = previousState == m_state && previousHeadline == m_headline
        && previousAlertCount == m_alertCount && previousTopAlert == m_topAlert;
    if (!unchanged) {
        Q_EMIT changed();
    }
    if (dirty) {
        requestSnapshotNow();
    }
}

void FirewallClient::handleAlertsReceived(const quint64 token, const QByteArray &json)
{
    const auto it = m_alertsCallbacks.find(token);
    if (it == m_alertsCallbacks.end()) {
        return;
    }
    const AlertsCallback callback = it.value();
    m_alertsCallbacks.erase(it);
    if (callback) {
        callback(decodeAlertList(json));
    }
}

void FirewallClient::handleAlertReceived(const quint64 token, const QByteArray &json)
{
    const auto it = m_alertCallbacks.find(token);
    if (it == m_alertCallbacks.end()) {
        return;
    }
    const AlertCallback callback = it.value();
    m_alertCallbacks.erase(it);
    if (!callback) {
        return;
    }
    Alert alert;
    callback(decodeAlert(json, &alert) ? std::optional<Alert>(std::move(alert)) : std::nullopt);
}

void FirewallClient::handleRequestFailed(const quint64 token, const QString &reason)
{
    Q_UNUSED(reason);
    if (m_snapshotToken.has_value() && *m_snapshotToken == token) {
        m_snapshotToken.reset();
        const bool dirty = m_snapshotDirty;
        m_snapshotDirty = false;
        // AGENT-NOTE: a failed fetch keeps last-known values rather than
        // flipping to Unavailable; only serviceAvailableChanged(false) does
        // that. Transient GetAll failures are common while the name owner
        // restarts and must not flash the panel to "off" each time.
        if (dirty) {
            requestSnapshotNow();
        }
        return;
    }
    if (const auto it = m_alertsCallbacks.find(token); it != m_alertsCallbacks.end()) {
        const AlertsCallback callback = it.value();
        m_alertsCallbacks.erase(it);
        if (callback) {
            callback({});
        }
        return;
    }
    if (const auto it = m_alertCallbacks.find(token); it != m_alertCallbacks.end()) {
        const AlertCallback callback = it.value();
        m_alertCallbacks.erase(it);
        if (callback) {
            callback(std::nullopt);
        }
    }
}

void FirewallClient::requestSnapshotNow()
{
    const quint64 token = nextToken();
    m_snapshotToken = token;
    m_snapshotDirty = false;
    m_transport.requestSnapshot(token);
}

void FirewallClient::publishStateIfChanged(const bool previousAvailable,
                                           const QString &previousState)
{
    if (previousAvailable != m_serviceAvailable || previousState != m_state) {
        Q_EMIT stateChanged();
    }
}

quint64 FirewallClient::nextToken()
{
    const quint64 result = m_nextToken++;
    if (m_nextToken == 0) {
        m_nextToken = 1;
    }
    return result;
}

} // namespace QindaQt::Firewall::Client
