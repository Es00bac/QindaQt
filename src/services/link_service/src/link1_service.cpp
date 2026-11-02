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
    // AGENT-GUARD: loopback only in this release. LAN mode (ADR-0434) adds a
    // second listener behind an explicit setting; never bind 0.0.0.0 here.
    if (!m_hub->listen(QHostAddress::LocalHost, m_config.port)) {
        return fail(QStringLiteral("cannot listen on 127.0.0.1:%1").arg(m_config.port));
    }
    m_messages = std::make_unique<MessagesFeature>([hub = m_hub.get()](const QString &id) { return hub->session(id); });
    m_hub->addFeature(m_messages.get());
    new MessagesAdaptor(m_messages.get(), m_bus); // owned by m_messages; exported below
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
                              QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals)) {
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

void Link1Service::ConfirmPairing(const QString &id, bool accept) { m_hub->confirmPairing(id, accept); }
void Link1Service::Unpair(const QString &id) { m_hub->unpair(id); }
void Link1Service::Ring(const QString &id)
{
    m_hub->ring(id, [id](bool ok, const QString &code) {
        if (!ok) qWarning("Link1 Ring failed for %.8s: %s", qPrintable(id), qPrintable(code));
    });
}
quint16 Link1Service::ListenPort() const { return m_hub->port(); }

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
