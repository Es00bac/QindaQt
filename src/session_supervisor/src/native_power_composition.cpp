// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_power_composition.h"
#include <qindaqt/platform/compositor_attachment/compositor_attachment.h>
#include <qindaqt/platform/idle_observation/idle_observation.h>
#include <qindaqt/services/power_client/idle_consumer_registrar.h>
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/session/display_power/scoped_display_power.h>
#include <qindaqt/session/display_power/display_power_facade.h>
#include <qindaqt/session/display_power/screen_power_service.h>
#include <qindaqt/session/idle_policy/shared_preferences.h>
#include <qindaqt/session/idle_policy/dim_stage.h>
#include <qindaqt/session/idle_policy/idle_suspend_stage.h>
#include <qindaqt/session/native_sleep/sleep_coordinator.h>
#include <qindaqt/session/native_lock_runtime/native_lock_runtime.h>
namespace QindaQt::SessionSupervisor {
using namespace QindaQt;
using namespace Session::IdlePolicy;
namespace {
class ComposedSleepPort final : public IdleSleepPort {
public:
    explicit ComposedSleepPort(Session::NativeSleep::SleepCoordinator &sleep) : m_sleep(sleep) {
        connect(&sleep, &Session::NativeSleep::SleepCoordinator::availabilityChanged,
                this, &IdleSleepPort::availabilityChanged);
        connect(&sleep, &Session::NativeSleep::SleepCoordinator::suspendFinished, this,
            [this](Session::NativeSleep::SleepResult result) {
                const auto token = m_token; m_token = 0;
                if (token) Q_EMIT requestFinished(token, result == Session::NativeSleep::SleepResult::Uncertain);
            });
    }
    bool available() const override { return m_sleep.canSuspend(); }
    void query(const QString &action, std::function<void(bool)> completion) override {
        m_sleep.queryCapability(mode(action), std::move(completion));
    }
    quint64 request(const QString &action) override {
        if (!m_sleep.requestSleep(mode(action))) return 0;
        m_token = m_sleep.requestToken(); return m_token;
    }
    void cancel(quint64 token) override {
        if (!token || token != m_token) return;
        // Clear after cancellation so synchronous uncertainty reaches stage.
        static_cast<void>(m_sleep.cancelRequest(token)); m_token = 0;
    }
private:
    static Session::NativeSleep::SleepMode mode(const QString &action) {
        return action == QStringLiteral("hibernate") ? Session::NativeSleep::SleepMode::Hibernate : Session::NativeSleep::SleepMode::Suspend;
    }
    Session::NativeSleep::SleepCoordinator &m_sleep;
    quint64 m_token = 0;
};
}
class NativePowerComposition::Private final : public QObject {
public:
    Private(QDBusConnection connection, Platform::Compositor::CompositorAttachment &peer,
        Power::PowerClient &client, Session::NativeLockRuntime::Runtime &lock,
        Session::NativeSleep::SleepCoordinator &coordinator)
        : bus(std::move(connection)), attachment(peer), power(client), runtime(lock), sleep(coordinator),
          transport(bus), settings(transport, sharedIdleSettingsKeys()), registrar(bus, power), sleepPort(sleep) {}
    std::optional<SharedIdlePreferences> preferences() const {
        if (!active || !attachment.live() || !attachment.sameBus(bus) ||
            settings.state() != Services::SettingsClient::ClientState::Ready || !settings.snapshot() ||
            settings.snapshot()->owner != settings.currentOwner() || !power.hasSnapshot() ||
            (power.state() != Power::PowerClientState::Ready && power.state() != Power::PowerClientState::Degraded)) return std::nullopt;
        const auto source = selectPowerSourceProfile(power.snapshot());
        if (!source) return std::nullopt;
        return sharedIdlePreferencesFor(*settings.snapshot(), settings.currentOwner(), *source);
    }
    void reconcile() {
        if (!active) return;
        if (facade) facade->refreshPreferences();
        if (dim) dim->refreshPreferences();
        if (display) display->refreshPreferences();
        if (suspend) suspend->refreshPreferences();
        const bool ready = preferences().has_value() && scoped && scoped->available();
        const auto owner = ready ? power.owner() : QString{};
        const auto epoch = ready ? power.snapshot().epoch : 0;
        if (owner == declaredOwner && epoch == declaredEpoch) return;
        registrar.cancel(); declaredOwner = owner; declaredEpoch = epoch;
        if (!owner.isEmpty() && epoch != 0) {
            // All consumers above are genuinely constructed, sharing this exact
            // ordinary attachment/Power1 source. Admission is not lease truth.
            static_cast<void>(registrar.declareConsumers(Power::IdleInhibitorScope::AutomaticLock |
                Power::IdleInhibitorScope::DisplayOff | Power::IdleInhibitorScope::IdleSuspend));
        }
    }
    QDBusConnection bus;
    Platform::Compositor::CompositorAttachment &attachment;
    Power::PowerClient &power;
    Session::NativeLockRuntime::Runtime &runtime;
    Session::NativeSleep::SleepCoordinator &sleep;
    Services::SettingsClient::QtSettingsTransport transport;
    Services::SettingsClient::SettingsClient settings;
    Power::IdleConsumerRegistrar registrar;
    ComposedSleepPort sleepPort;
    std::unique_ptr<Session::DisplayPower::ScopedDisplayPower> scoped;
    std::unique_ptr<Session::DisplayPower::DisplayPowerFacade> facade;
    std::unique_ptr<Session::DisplayPower::ScreenPowerService> service;
    std::unique_ptr<Platform::Idle::WaylandIdleObservation> dimIdle, displayIdle, suspendIdle;
    std::unique_ptr<DimStage> dim;
    std::unique_ptr<DisplayOffStage> display;
    std::unique_ptr<IdleSuspendStage> suspend;
    QString declaredOwner;
    quint64 declaredEpoch = 0;
    bool active = false;
};
NativePowerComposition::NativePowerComposition(QDBusConnection bus,
    Platform::Compositor::CompositorAttachment &attachment, Power::PowerClient &power,
    Session::NativeLockRuntime::Runtime &lock, Session::NativeSleep::SleepCoordinator &sleep)
    : d(std::make_unique<Private>(std::move(bus), attachment, power, lock, sleep)) {}
NativePowerComposition::~NativePowerComposition() { stop(); }
bool NativePowerComposition::start() {
    if (d->active) return true;
    const auto identity = d->attachment.identity();
    if (!identity || !d->attachment.sameBus(d->bus) || !d->attachment.live()) return false;
    d->active = true;
    d->scoped = std::make_unique<Session::DisplayPower::ScopedDisplayPower>(d->bus,
        identity->compositorOwner, static_cast<quint32>(identity->compositorPid),
        [this] { return d->active && d->attachment.live() && d->attachment.sameBus(d->bus); }, true);
    d->facade = std::make_unique<Session::DisplayPower::DisplayPowerFacade>(*d->scoped, d->power, d->runtime,
        [this]() -> std::optional<bool> { const auto p = d->preferences(); return p ? std::optional(p->lockBeforeDisplayOff) : std::nullopt; });
    d->service = std::make_unique<Session::DisplayPower::ScreenPowerService>(d->bus, *d->facade);
    const auto observer = [this] { return std::make_unique<Platform::Idle::WaylandIdleObservation>(
        [this] { return d->attachment.openConnection(); }, [this] { return d->active && d->attachment.live(); }); };
    d->dimIdle = observer(); d->displayIdle = observer(); d->suspendIdle = observer();
    d->dim = std::make_unique<DimStage>(*d->dimIdle, d->power, [this] { return d->preferences(); });
    d->display = std::make_unique<DisplayOffStage>(*d->displayIdle, *d->facade, d->power,
        [this]() -> std::optional<DisplayOffPreferences> { const auto p = d->preferences(); return p ? std::optional(p->display) : std::nullopt; });
    d->suspend = std::make_unique<IdleSuspendStage>(*d->suspendIdle, d->power, d->sleepPort, [this] { return d->preferences(); });
    QObject::connect(&d->settings, &Services::SettingsClient::SettingsClient::snapshotChanged, d.get(), [this] { d->reconcile(); });
    QObject::connect(&d->settings, &Services::SettingsClient::SettingsClient::ownerChanged, d.get(), [this] { d->reconcile(); });
    QObject::connect(&d->settings, &Services::SettingsClient::SettingsClient::stateChanged, d.get(), [this] { d->reconcile(); });
    QObject::connect(&d->power, &Power::PowerClient::snapshotChanged, d.get(), [this] { d->reconcile(); });
    QObject::connect(&d->power, &Power::PowerClient::stateChanged, d.get(), [this] { d->reconcile(); });
    QObject::connect(d->scoped.get(), &Session::DisplayPower::ScopedDisplayPower::availabilityChanged, d.get(), [this] { d->reconcile(); });
    QObject::connect(&d->attachment, &Platform::Compositor::CompositorAttachment::revoked, d.get(), [this] { stop(); });
    if (!d->service->start() || !d->settings.start() || !d->scoped->start()) { stop(); return false; }
    d->dim->start(); d->display->start(); d->suspend->start(); d->reconcile(); return true;
}
void NativePowerComposition::stop() {
    if (!d->active) return;
    d->active = false; d->registrar.cancel();
    d->declaredOwner.clear(); d->declaredEpoch = 0;
    if (d->suspend) d->suspend->stop();
    if (d->display) d->display->stop();
    if (d->dim) d->dim->stop();
    if (d->service) d->service->stop();
    if (d->facade) d->facade->restoreAndStop();
    d->settings.stop();
    QObject::disconnect(&d->settings, nullptr, d.get(), nullptr);
    QObject::disconnect(&d->power, nullptr, d.get(), nullptr);
    QObject::disconnect(&d->attachment, nullptr, d.get(), nullptr);
    d->suspend.reset(); d->display.reset();
    // Dim's outstanding restore is retained until destruction below; no new
    // authority is acquired on replacement. PowerClient fences its own reply.
    d->dim.reset(); d->service.reset(); d->facade.reset(); d->scoped.reset();
    d->dimIdle.reset(); d->displayIdle.reset(); d->suspendIdle.reset();
}
}
