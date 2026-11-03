// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_service/link1_service.h>

#include <QtCore/QJsonDocument>
#include <QtCore/QSysInfo>

namespace QindaQt::Link {
namespace {
constexpr auto kName = "org.qindaqt.Link1";
constexpr auto kPath = "/org/qindaqt/Link1";
}

Link1Service::Link1Service(ServiceConfig config, QDBusConnection bus, PairingPresenter *presenter,
                           AdbRunner *adb, QObject *parent)
    : QObject(parent), m_config(std::move(config)), m_bus(std::move(bus)), m_presenter(presenter), m_adb(adb)
{
}

Link1Service::~Link1Service()
{
    if (m_bus.isConnected()) {
        m_bus.unregisterObject(QString::fromLatin1(kPath));
        m_bus.unregisterObject(QStringLiteral("/org/qindaqt/Link1/Messages"));
        m_bus.unregisterService(QString::fromLatin1(kName));
    }
}

bool Link1Service::start(QString *error)
{
    auto fail = [error](const QString &why) {
        if (error != nullptr) *error = why;
        return false;
    };
    if (m_config.callerAuthority == nullptr) {
        return fail(QStringLiteral("no caller authority configured; refusing to export Link1 unprotected"));
    }
    IdentityStore identityStore(m_config.dataDir);
    QString identityError;
    const DeviceIdentity identity = identityStore.loadOrCreate(QSysInfo::machineHostName(), &identityError);
    if (!identity.isValid()) {
        return fail(identityError);
    }
    m_store = std::make_unique<TrustStore>(m_config.dataDir);
    if (!m_store->load()) {
        return fail(QStringLiteral("devices.json is unreadable; refusing to start with an empty trust store"));
    }
    HubOptions options;
    options.local.name = QSysInfo::machineHostName();
    options.local.model = QStringLiteral("QindaQt");
    options.local.os = QSysInfo::prettyProductName();
    options.local.app = m_config.version;
    m_hub = std::make_unique<Hub>(identity, *m_store, options);
    // AGENT-GUARD: this is the loopback listener only. LAN mode (ADR-0434, ADR-0440) is the
    // LanController below, behind an explicit persisted setting; never bind 0.0.0.0 here.
    if (!m_hub->listen(QHostAddress::LocalHost, m_config.port)) {
        return fail(QStringLiteral("cannot listen on 127.0.0.1:%1").arg(m_config.port));
    }
    {
        LanOptions lan;
        lan.configDir = m_config.configDir.isEmpty() ? m_config.dataDir + QStringLiteral("/config") : m_config.configDir;
        lan.port = m_config.lanPort;
        lan.addresses = m_config.lanAddresses;
        lan.publisher = m_config.lanPublisher;
        lan.instanceName = QSysInfo::machineHostName();
        lan.pairingWindowMs = m_config.pairingWindowMs;
        m_lan = std::make_unique<LanController>(*m_hub, std::move(lan));
        connect(m_lan.get(), &LanController::pairingWindowChanged, this, &Link1Service::PairingWindowChanged);
        connect(m_lan.get(), &LanController::statusChanged, this, [this] { Q_EMIT LanStatusChanged(m_lan->statusJson()); });
    }
    if (m_config.phoneNotifications != nullptr) {
        new NotificationsModule(*m_hub,
                                {m_config.configDir.isEmpty() ? m_config.dataDir + QStringLiteral("/config") : m_config.configDir,
                                 m_config.cacheDir.isEmpty() ? m_config.dataDir + QStringLiteral("/cache") : m_config.cacheDir,
                                 m_config.phoneNotifications, m_config.replyLauncher, m_config.callerAuthority, m_bus},
                                this);
    }
    m_messages = std::make_unique<MessagesFeature>([hub = m_hub.get()](const QString &id) { return hub->session(id); });
    m_hub->addFeature(m_messages.get());
    m_sendGuard = std::make_unique<SendGuard>(m_config.phoneNotifications, SendGuard::Clock{},
                                              [this](const QString &id) { return m_hub->session(id) != nullptr || m_store->contains(id); });
    new MessagesAdaptor(m_messages.get(), m_bus, m_config.callerAuthority, m_sendGuard.get()); // owned by m_messages; exported below
    connect(m_hub.get(), &Hub::devicesChanged, this, &Link1Service::onDevicesChanged);
    connect(m_hub.get(), &Hub::pairingRequested, this,
            [this](const QString &id, const QString &name, const QString &model, const QString &code) {
                m_presented.append(id);
                if (m_presenter != nullptr) {
                    m_presenter->present(id, name, model, code);
                }
                Q_EMIT PairingRequested(id, name, model, code);
            });
    if (m_presenter != nullptr) {
        connect(m_presenter, &PairingPresenter::decided, m_hub.get(), &Hub::confirmPairing);
    }
    if (m_adb != nullptr) {
        TunnelOptions tunnel;
        tunnel.port = m_config.port == 0 ? m_hub->port() : m_config.port;
        m_tunnels = std::make_unique<TunnelManager>(*m_adb, tunnel);
        m_tunnels->start();
    }
    if (!m_bus.registerObject(QString::fromLatin1(kPath), this,
                              QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals
                                  | QDBusConnection::ExportScriptableProperties
                                  | QDBusConnection::ExportAdaptors)) {
        return fail(QStringLiteral("cannot register %1").arg(QLatin1String(kPath)));
    }
    if (!m_bus.registerObject(QStringLiteral("/org/qindaqt/Link1/Messages"), m_messages.get(),
                              QDBusConnection::ExportAdaptors)) {
        m_bus.unregisterObject(QString::fromLatin1(kPath));
        return fail(QStringLiteral("cannot register the Messages object"));
    }
    if (!m_bus.registerService(QString::fromLatin1(kName))) {
        m_bus.unregisterObject(QString::fromLatin1(kPath));
        return fail(QStringLiteral("%1 is already owned").arg(QLatin1String(kName)));
    }
    return true;
}

QString Link1Service::Devices() const
{
    return QString::fromUtf8(QJsonDocument(m_hub->devices()).toJson(QJsonDocument::Compact));
}

// AGENT-GUARD (ADR-0438): ConfirmPairing, Unpair and Ring act as the user; they need a trusted caller.
// Devices, PendingPairings and ListenPort are public metadata and stay open.
void Link1Service::ConfirmPairing(const QString &id, bool accept)
{
    if (!m_config.callerAuthority->authorize(message())) return;
    m_hub->confirmPairing(id, accept);
}
void Link1Service::Unpair(const QString &id)
{
    if (!m_config.callerAuthority->authorize(message())) return;
    m_hub->unpair(id);
}
QString Link1Service::PendingPairings() const
{
    return QString::fromUtf8(QJsonDocument(m_hub->pendingPairings()).toJson(QJsonDocument::Compact));
}
void Link1Service::Ring(const QString &id)
{
    if (!m_config.callerAuthority->authorize(message())) return;
    m_hub->ring(id, [id](bool ok, const QString &code) {
        if (!ok) qWarning("Link1 Ring failed for %.8s: %s", qPrintable(id), qPrintable(code));
    });
}
quint16 Link1Service::ListenPort() const { return m_hub->port(); }

// AGENT-GUARD (ADR-0438, ADR-0440): both act as the user on the machine's network exposure.
void Link1Service::SetLanEnabled(bool enabled)
{
    if (!m_config.callerAuthority->authorize(message())) return;
    m_lan->setEnabled(enabled);
}
quint32 Link1Service::OpenPairingWindow()
{
    if (!m_config.callerAuthority->authorize(message())) return 0;
    return m_lan->openPairingWindow();
}
bool Link1Service::LanEnabled() const { return m_lan->enabled(); }
QString Link1Service::LanStatus() const { return m_lan->statusJson(); }

void Link1Service::onDevicesChanged()
{
    // Retire notifications whose pairing ended (accepted, rejected, timed out, disconnected).
    for (const QString &id : std::as_const(m_presented)) {
        Session *s = m_hub->session(id);
        if (s == nullptr || !s->pairingPending()) {
            if (m_presenter != nullptr) m_presenter->dismiss(id);
            m_presented.removeAll(id);
        }
    }
    Q_EMIT DevicesChanged(Devices());
}

} // namespace QindaQt::Link
