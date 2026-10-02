#include "native_lock_composition.h"
#include "native_sleep_composition.h"
#include "native_power_composition.h"
#include <qindaqt/compositor_names/compositor_names.h>
#include <qindaqt/platform/compositor_attachment/compositor_attachment.h>
#include <qindaqt/platform/idle_observation/idle_observation.h>
#include <qindaqt/services/lock_preferences/lock_preferences.h>
#include <qindaqt/services/native_lock_service/qt_native_lock_request.h>
#include <qindaqt/services/native_lock_service/resident_lock_service.h>
#include <qindaqt/services/power_client/power_client.h>
#include <qindaqt/services/power_client/qt_power_transport.h>
#include <qindaqt/services/session_lock_state/native_lock_state_monitor.h>
#include <qindaqt/services/session_lock_state/qt_native_lock_transport.h>
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/session/native_lock_runtime/native_lock_runtime.h>
#include <qindaqt/session/idle_policy/display_off_stage.h>
#include <qindaqt/session/idle_policy/source_preferences.h>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QDebug>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <unistd.h>
#include <utility>

namespace QindaQt::SessionSupervisor {
using namespace QindaQt;
using Platform::Compositor::CompositorAttachment;
using namespace Services::SessionLockState;

class NativeLockComposition::Private final {
public:
  QDBusConnection bus = QDBusConnection::sessionBus();
  QString sessionOwner;
  std::unique_ptr<CompositorAttachment> attachment;
  std::unique_ptr<Platform::Idle::WaylandIdleObservation> idle;
  std::unique_ptr<NativePowerComposition> nativePower;
  std::unique_ptr<QtNativeLockTransport> lockTransport;
  std::unique_ptr<NativeLockStateMonitor> monitor;
  std::unique_ptr<Services::NativeLock::QtNativeLockRequest> request;
  std::unique_ptr<Services::NativeLock::ResidentLockService> service;
  std::unique_ptr<Services::SettingsClient::QtSettingsTransport> settingsTransport;
  std::unique_ptr<Services::SettingsClient::SettingsClient> settings;
  std::unique_ptr<Services::LockPreferences::PreferencesProvider> preferences;
  std::unique_ptr<Power::QtPowerTransport> powerTransport;
  std::unique_ptr<Power::PowerClient> power;
  std::unique_ptr<Session::NativeLockRuntime::Runtime> runtime;
  std::unique_ptr<NativeSleepComposition> sleep;
  bool active = false;
};

NativeLockComposition::NativeLockComposition() : d(std::make_unique<Private>()) {}
NativeLockComposition::~NativeLockComposition() { stop(); }

bool NativeLockComposition::start(QString *error, bool nativePowerExclusive) {
  if (d->active) return true;
  const auto fail = [error](const QString &reason) {
    if (error) *error = reason;
    return false;
  };
  if (!d->bus.isConnected() || !d->bus.interface())
    return fail(QStringLiteral("session bus is unavailable"));
  const auto sessionOwner = d->bus.interface()->serviceOwner(QStringLiteral("org.qindaqt.Session1"));
  if (!sessionOwner.isValid() || sessionOwner.value().isEmpty())
    return fail(QStringLiteral("supervised Session1 owner is unavailable"));
  const auto sessionPid = d->bus.interface()->servicePid(sessionOwner.value());
  if (!sessionPid.isValid() || sessionPid.value() != static_cast<uint>(getpid()))
    return fail(QStringLiteral("Session1 owner is not this supervisor process"));
  d->sessionOwner = sessionOwner.value();

  const auto compositorOwner = d->bus.interface()->serviceOwner(QString(CompositorNames::service));
  if (!compositorOwner.isValid() || compositorOwner.value().isEmpty())
    return fail(QStringLiteral("compositor owner is unavailable"));
  const auto compositorPid = d->bus.interface()->servicePid(compositorOwner.value());
  if (!compositorPid.isValid() || compositorPid.value() <= 1)
    return fail(QStringLiteral("compositor PID is unavailable"));

  const QString runtimeDirectory = QProcessEnvironment::systemEnvironment().value(QStringLiteral("XDG_RUNTIME_DIR"));
  const QString socket = QProcessEnvironment::systemEnvironment().value(QStringLiteral("WAYLAND_DISPLAY"));
  static const QRegularExpression socketPattern(QStringLiteral("^qindaqt-[0-9]+$"));
  if (runtimeDirectory.isEmpty() || !socketPattern.match(socket).hasMatch())
    return fail(QStringLiteral("selected QindaQt Wayland socket is unavailable"));

  d->attachment = std::make_unique<CompositorAttachment>(
      d->bus, runtimeDirectory, [this](const QString &owner) {
        if (!d->bus.interface() || owner != d->sessionOwner) return false;
        const auto current = d->bus.interface()->serviceOwner(QStringLiteral("org.qindaqt.Session1"));
        const auto pid = d->bus.interface()->servicePid(owner);
        return current.isValid() && current.value() == owner && pid.isValid() &&
               pid.value() == static_cast<uint>(getpid());
      });
  if (!d->attachment->attach(d->sessionOwner, socket,
          Platform::Compositor::PeerExpectation{
              compositorOwner.value(), static_cast<quint64>(compositorPid.value())}))
    return fail(QStringLiteral("ordinary compositor attachment failed"));

  d->idle = std::make_unique<Platform::Idle::WaylandIdleObservation>(
      [this] { return d->attachment->openConnection(); },
      [this] { return d->attachment && d->attachment->live(); });
  d->lockTransport = std::make_unique<QtNativeLockTransport>(d->bus);
  QString transportError;
  if (!d->lockTransport->start(&transportError))
    return fail(transportError);
  d->monitor = std::make_unique<NativeLockStateMonitor>(
      *d->lockTransport, [this](const QString &owner, const quint64 pid) {
        const auto identity = d->attachment->identity();
        return identity && d->attachment->sameBus(d->bus) &&
               identity->compositorOwner == owner && identity->compositorPid == pid;
      });
  if (!d->monitor->start(&transportError)) return fail(transportError);
  d->request = std::make_unique<Services::NativeLock::QtNativeLockRequest>(
      d->bus, *d->attachment);
  d->service = std::make_unique<Services::NativeLock::ResidentLockService>(
      d->bus, *d->monitor, *d->request);
  if (!d->service->start(&transportError)) return fail(transportError);

  d->settingsTransport = std::make_unique<Services::SettingsClient::QtSettingsTransport>(d->bus);
  d->settings = std::make_unique<Services::SettingsClient::SettingsClient>(
      *d->settingsTransport, Services::LockPreferences::scopedKeys());
  if (!d->settings->start(&transportError)) return fail(transportError);
  d->preferences = std::make_unique<Services::LockPreferences::PreferencesProvider>(*d->settings);
  d->powerTransport = std::make_unique<Power::QtPowerTransport>(d->bus);
  d->power = std::make_unique<Power::PowerClient>(d->powerTransport.get());
  d->runtime = std::make_unique<Session::NativeLockRuntime::Runtime>(
      *d->preferences, *d->idle, *d->request, *d->monitor, d->power.get());
  d->runtime->start();
  d->sleep = std::make_unique<NativeSleepComposition>(d->bus, *d->runtime, *d->monitor,
      [this] { return d->attachment && d->attachment->sameBus(d->bus) &&
                      d->attachment->live(); });
  if (!d->sleep->start())
    return fail(QStringLiteral("native sleep handoff registration failed"));

  if (nativePowerExclusive) {
    d->nativePower = std::make_unique<NativePowerComposition>(
        d->bus, *d->attachment, *d->power, *d->runtime, d->sleep->coordinator(),
        [this] {
          const auto values = d->preferences->preferences();
          return values && (!values->automaticLock || d->runtime->available());
        });
    if (!d->nativePower->start()) {
      stop();
      return fail(QStringLiteral("exclusive native power prerequisites/receipts unavailable"));
    }
  }
  d->active = true;
  if (error) error->clear();
  return true;
}

void NativeLockComposition::stop() {
  if (!d) return;
  if (d->nativePower) d->nativePower->stop();
  d->nativePower.reset();
  if (d->sleep) d->sleep->stop();
  d->sleep.reset();
  if (d->runtime) d->runtime->stop();
  d->runtime.reset();
  if (d->service) d->service->stop();
  d->service.reset();
  d->request.reset();
  if (d->monitor) d->monitor->stop();
  d->monitor.reset();
  if (d->lockTransport) d->lockTransport->stop();
  d->lockTransport.reset();
  d->power.reset();
  d->powerTransport.reset();
  d->preferences.reset();
  if (d->settings) d->settings->stop();
  d->settings.reset();
  d->settingsTransport.reset();
  d->idle.reset();
  if (d->attachment) d->attachment->revoke();
  d->attachment.reset();
  d->active = false;
}
}
