// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/voice_configuration/voice_configuration_client.h>
#include <qindaqt/services/voice_configuration/voice_configuration_codec.h>
#include <algorithm>
#include <limits>
namespace QindaQt::Services::VoiceConfiguration {
Client::Client(Transport &transport, QObject *parent, int operationTimeoutMs) : QObject(parent), m_transport(transport) {
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(std::clamp(operationTimeoutMs, 1, 20000));
    connect(&m_timeout, &QTimer::timeout, this, &Client::uncertain);
    connect(&transport, &Transport::ownerChanged, this, [this](const QString &owner) {
        if (!m_running || owner == m_owner) return;
        const bool wasBusy = busy();
        m_timeout.stop(); m_operationToken = 0; m_fetchToken = 0;
        m_owner = owner; m_ready = false; m_snapshot = {};
        m_status = wasBusy ? QStringLiteral("uncertain") : QStringLiteral("unavailable");
        Q_EMIT entryClearRequested(); Q_EMIT changed();
        if (!owner.isEmpty()) refresh();
    });
    connect(&transport, &Transport::invalidated, this, [this](const QString &owner) {
        if (m_running && owner == m_owner && !busy()) refresh();
    });
    connect(&transport, &Transport::snapshotReply, this,
        [this](const QString &owner, quint64 token, bool success, bool unsupported, const QVariantMap &map) {
            if (!m_running || owner != m_owner || token != m_fetchToken || token == 0) return;
            m_fetchToken = 0;
            Snapshot value;
            if (!success || !decodeSnapshot(map, value)) {
                m_ready = false; m_status = unsupported ? QStringLiteral("unsupported") : QStringLiteral("unavailable");
            } else if (m_snapshot.revision != 0 && (value.revision < m_snapshot.revision
                       || (value.revision == m_snapshot.revision && value != m_snapshot))) {
                m_ready = false; m_status = QStringLiteral("unavailable");
            } else {
                m_snapshot = value; m_ready = true;
                // An explicit reload is required after uncertainty, never replay.
                if (m_status != QStringLiteral("uncertain")) m_status = value.statusCode;
            }
            Q_EMIT changed();
        });
    connect(&transport, &Transport::operationReply, this,
        [this](const QString &owner, quint64 token, bool success, const QVariantMap &map) {
            if (!m_running || owner != m_owner || token != m_operationToken || token == 0) return;
            Result value;
            if (!success || !decodeResult(map, value) || value.requestId != m_request
                || value.operation != m_operation || value.revision < m_snapshot.revision) {
                uncertain(); return;
            }
            m_timeout.stop(); m_operationToken = 0;
            m_status = value.status == Status::Uncertain ? QStringLiteral("uncertain") : value.reasonCode;
            m_ready = false; Q_EMIT changed(); refresh();
        });
}
Client::~Client() { stop(); }
void Client::start() { if (!m_running) { m_running = true; m_transport.start(); } }
void Client::stop() {
    m_running = false; m_timeout.stop(); m_transport.stop();
    m_owner.clear(); m_snapshot = {}; m_ready = false; m_fetchToken = 0; m_operationToken = 0;
    m_status = QStringLiteral("unavailable"); Q_EMIT entryClearRequested(); Q_EMIT changed();
}
void Client::refresh() {
    if (!m_running || m_owner.isEmpty() || busy()) return;
    if (m_next == std::numeric_limits<quint64>::max()) return;
    m_fetchToken = ++m_next; m_transport.fetch(m_owner, m_fetchToken);
}
bool Client::save(const QString &key) { return submit(Operation::SaveElevenLabsKey, key); }
bool Client::reload() { return submit(Operation::ReloadCredentials, {}); }
bool Client::submit(Operation operation, const QString &key) {
    Q_EMIT entryClearRequested();
    if (m_next > std::numeric_limits<quint64>::max() - 2 || !m_running || !m_ready || !m_snapshot.canConfigure || busy()
        || (operation == Operation::SaveElevenLabsKey && !validKey(key))) return false;
    m_fetchToken = 0; m_operation = operation; m_request = ++m_next;
    m_operationToken = ++m_next; m_status = QStringLiteral("busy");
    m_timeout.start(); Q_EMIT changed();
    m_transport.submit(m_owner, m_operationToken, m_request, m_snapshot.revision, operation, key);
    return true;
}
void Client::uncertain() {
    if (!busy()) return;
    m_timeout.stop(); m_operationToken = 0; m_ready = false;
    m_status = QStringLiteral("uncertain"); Q_EMIT changed(); refresh();
}
}
