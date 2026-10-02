// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/session/display_power/screen_power_service.h>
#include <qindaqt/session/display_power/display_power_facade.h>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusReply>
#include <utility>
namespace QindaQt::Session::DisplayPower {
namespace {
const QString Service = QStringLiteral("org.qindaqt.ScreenPower1");
const QString Path = QStringLiteral("/org/qindaqt/ScreenPower1");
}
class ScreenPowerService::Endpoint final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.ScreenPower1")
public:
    Endpoint(QDBusConnection bus, DisplayPowerFacade &facade) : m_bus(std::move(bus)), m_facade(facade) {
        connect(&facade, &IdlePolicy::DisplayPowerPort::availabilityChanged, this, [this](bool) { Q_EMIT AvailabilityChanged(); });
        connect(&facade, &DisplayPowerFacade::screenOffFinished, this, [this](const QString &id, bool admitted) {
            if (m_pending.type() != QDBusMessage::MethodCallMessage || id != m_id) return;
            m_bus.send(m_pending.createReply(QVariantList{admitted})); m_pending = QDBusMessage{};
        });
        connect(&facade, &DisplayPowerFacade::screenOffEnded, this, [this](const QString &id) {
            if (id != m_id || m_owner.isEmpty()) return;
            auto ended = QDBusMessage::createTargetedSignal(m_owner, Path, Service, QStringLiteral("ScreenOffEnded"));
            ended << id; m_bus.send(ended);
        });
    }
public Q_SLOTS:
    bool CanScreenOff() { return calledFromDBus() && m_facade.callerAllowed(message().service()); }
    bool ScreenOff(qulonglong powerEpoch, const QString &id) {
        if (!calledFromDBus() || !m_facade.callerAllowed(message().service())
            || m_facade.screenOffEpisodeHeld() || m_pending.type() == QDBusMessage::MethodCallMessage) return false;
        m_owner = message().service(); m_id = id; m_pending = message(); setDelayedReply(true);
        if (!m_facade.requestScreenOff(m_owner, powerEpoch, id)
            && m_pending.type() == QDBusMessage::MethodCallMessage) {
            m_bus.send(m_pending.createReply(QVariantList{false})); m_pending = QDBusMessage{};
        }
        return false; // The actual cause-admission reply is explicitly delayed.
    }
    bool ReleaseScreenOff(const QString &id) {
        return calledFromDBus() && m_facade.releaseScreenOff(message().service(), id);
    }
Q_SIGNALS:
    void AvailabilityChanged();
    void ScreenOffEnded(const QString &id);
private:
    QDBusConnection m_bus;
    DisplayPowerFacade &m_facade;
    QDBusMessage m_pending;
    QString m_id, m_owner;
};
ScreenPowerService::ScreenPowerService(QDBusConnection bus, DisplayPowerFacade &facade, QObject *parent)
    : QObject(parent), m_bus(std::move(bus)), m_facade(facade),
      m_sessionWatch(QStringLiteral("org.qindaqt.Session1"), m_bus, QDBusServiceWatcher::WatchForOwnerChange) {
    connect(&m_sessionWatch, &QDBusServiceWatcher::serviceOwnerChanged, this, [this] {
        if (!m_bus.interface()) { stop(); return; }
        const auto owner = m_bus.interface()->serviceOwner(QStringLiteral("org.qindaqt.Session1"));
        if (!owner.isValid() || owner.value() != m_bus.baseService()) stop();
    });
}
ScreenPowerService::~ScreenPowerService() { stop(); }
bool ScreenPowerService::start() {
    if (m_active) return true;
    if (!m_bus.interface()) return false;
    const auto owner = m_bus.interface()->serviceOwner(QStringLiteral("org.qindaqt.Session1"));
    if (!owner.isValid() || owner.value() != m_bus.baseService()) return false;
    m_endpoint = std::make_unique<Endpoint>(m_bus, m_facade);
    if (!m_bus.registerObject(Path, m_endpoint.get(), QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)) {
        m_endpoint.reset(); return false;
    }
    if (!m_bus.registerService(Service)) { m_bus.unregisterObject(Path); m_endpoint.reset(); return false; }
    m_active = true; return true;
}
void ScreenPowerService::stop() {
    if (!m_active) return;
    m_facade.restoreAndStop();
    m_bus.unregisterObject(Path); m_bus.unregisterService(Service); m_endpoint.reset(); m_active = false;
}
}
#include "screen_power_service.moc"
