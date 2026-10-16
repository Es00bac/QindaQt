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
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDebug>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTimer>
#include <optional>
#include <utility>

namespace QindaQt::SessionSupervisor {
using namespace QindaQt;
using Platform::Compositor::CompositorAttachment;
using namespace Services::SessionLockState;

namespace {
const QString SessionName = QStringLiteral("org.qindaqt.Session1");
// AGENT-NOTE: explicit per-call bounds; never the connection's shared
// interface timeout, which other session components also use (ADR-0379).
constexpr int DaemonCallTimeoutMs = 1000;
QDBusMessage daemonCall(const QString &method, const QString &argument) {
  auto message = QDBusMessage::createMethodCall(
      QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
      QStringLiteral("org.freedesktop.DBus"), method);
  message << argument;
  return message;
}
std::optional<QString> nameOwner(const QDBusConnection &bus, const QString &name) {
  const QDBusReply<QString> reply =
      bus.call(daemonCall(QStringLiteral("GetNameOwner"), name), QDBus::Block, DaemonCallTimeoutMs);
  return reply.isValid() && !reply.value().isEmpty() ? std::optional(reply.value()) : std::nullopt;
}
std::optional<uint> ownerPid(const QDBusConnection &bus, const QString &owner) {
  const QDBusReply<uint> reply = bus.call(
      daemonCall(QStringLiteral("GetConnectionUnixProcessID"), owner), QDBus::Block,
      DaemonCallTimeoutMs);
  return reply.isValid() ? std::optional(reply.value()) : std::nullopt;
}
}

class NativeLockComposition::Private final {
public:
  QDBusConnection bus = QDBusConnection::sessionBus();
  QString sessionOwner;
  // Cached from NameOwnerChanged; attachment admission never calls the bus.
  bool session1Owned = false;
  bool nativePowerExclusive = false, nativePowerStopped = false, nativePowerRestartQueued = false;
  std::unique_ptr<QDBusServiceWatcher> sessionWatcher;
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

  // AGENT-CONTRACT (ADR-0379): the attachment re-attaches by itself after
  // genuine loss. Every consumer it feeds follows it here, so idle and manual
  // lock, the Lock1 and screensaver facades and sleep admission recover
  // without a new login. Loss publishes Unknown; re-attachment re-probes
  // immediately.
  void attachmentChanged() {
    const bool live = attachment && attachment->live();
    if (monitor) monitor->refresh();
    if (idle) {
      if (live) idle->refresh();
      else idle->revoke();
    }
    if (sleep) sleep->admissionChanged();
    if (live && nativePowerStopped) queueNativePowerRestart();
  }
  void queueNativePowerRestart() {
    if (!nativePowerExclusive || !nativePower || nativePowerRestartQueued) return;
    nativePowerRestartQueued = true;
    // Native power start runs its own bounded readiness loop; never nest it
    // inside the attachment's signal emission.
    QTimer::singleShot(0, attachment.get(), [this] {
      nativePowerRestartQueued = false;
      if (!active || !nativePowerStopped || !attachment || !attachment->live()) return;
      if (nativePower->start()) {
        nativePowerStopped = false;
        qInfo("qindaqt-session: native power composition resumed after re-attachment");
      } else {
        qWarning("qindaqt-session: native power composition did not resume after re-attachment");
      }
    });
  }
};

NativeLockComposition::NativeLockComposition() : d(std::make_unique<Private>()) {}
NativeLockComposition::~NativeLockComposition() { stop(); }

bool NativeLockComposition::start(QString *error, bool nativePowerExclusive) {
  if (d->active) return true;
  const auto fail = [this, error](const QString &reason) {
    // Leave no partial assembly behind, so a later start() is a clean retry.
    stop();
    if (error) *error = reason;
    return false;
  };
  if (!d->bus.isConnected())
    return fail(QStringLiteral("session bus is unavailable"));
  // Session1 must belong to this very connection: SessionService registered
  // it here, so the unique name itself proves the supervisor process.
  const auto sessionOwner = nameOwner(d->bus, SessionName);
  if (!sessionOwner || *sessionOwner != d->bus.baseService())
    return fail(QStringLiteral("supervised Session1 owner is unavailable"));
  d->sessionOwner = *sessionOwner;
  d->session1Owned = true;
  d->sessionWatcher = std::make_unique<QDBusServiceWatcher>(
      SessionName, d->bus, QDBusServiceWatcher::WatchForOwnerChange);
  QObject::connect(d->sessionWatcher.get(), &QDBusServiceWatcher::serviceOwnerChanged,
                   d->sessionWatcher.get(),
                   [this](const QString &, const QString &, const QString &newOwner) {
    const bool owned = newOwner == d->sessionOwner;
    if (owned == d->session1Owned) return;
    d->session1Owned = owned;
    if (d->sleep) d->sleep->admissionChanged();
  });

  const auto compositorOwner = nameOwner(d->bus, QString(CompositorNames::service));
  if (!compositorOwner)
    return fail(QStringLiteral("compositor owner is unavailable"));
  const auto compositorPid = ownerPid(d->bus, *compositorOwner);
  if (!compositorPid || *compositorPid <= 1)
    return fail(QStringLiteral("compositor PID is unavailable"));

  const QString runtimeDirectory = QProcessEnvironment::systemEnvironment().value(QStringLiteral("XDG_RUNTIME_DIR"));
  const QString socket = QProcessEnvironment::systemEnvironment().value(QStringLiteral("WAYLAND_DISPLAY"));
  static const QRegularExpression socketPattern(QStringLiteral("^qindaqt-[0-9]+$"));
  if (runtimeDirectory.isEmpty() || !socketPattern.match(socket).hasMatch())
    return fail(QStringLiteral("selected QindaQt Wayland socket is unavailable"));

  d->attachment = std::make_unique<CompositorAttachment>(
      d->bus, runtimeDirectory, [this](const QString &owner) {
        return d->session1Owned && owner == d->sessionOwner;
      });
  if (!d->attachment->attach(d->sessionOwner, socket,
          Platform::Compositor::PeerExpectation{
              *compositorOwner, static_cast<quint64>(*compositorPid)}))
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
      [this] { return d->session1Owned && d->attachment && d->attachment->live(); });
  if (!d->sleep->start())
    return fail(QStringLiteral("native sleep handoff registration failed"));

  d->nativePowerExclusive = nativePowerExclusive;
  if (nativePowerExclusive) {
    d->nativePower = std::make_unique<NativePowerComposition>(
        d->bus, *d->attachment, *d->power, *d->runtime, d->sleep->coordinator(),
        [this] {
          const auto values = d->preferences->preferences();
          return values && (!values->automaticLock || d->runtime->available());
        });
    if (!d->nativePower->start())
      return fail(QStringLiteral("exclusive native power prerequisites/receipts unavailable"));
  }
  // Wire recovery only after the first attach: its synchronous attached()
  // emission precedes every consumer above.
  QObject::connect(d->attachment.get(), &CompositorAttachment::revoked,
                   d->attachment.get(), [this] {
    // NativePowerComposition stops itself on revocation; resume it later.
    if (d->nativePower) d->nativePowerStopped = true;
    d->attachmentChanged();
  });
  QObject::connect(d->attachment.get(), &CompositorAttachment::attached,
                   d->attachment.get(), [this] { d->attachmentChanged(); });
  d->active = true;
  if (error) error->clear();
  return true;
}

void NativeLockComposition::stop() {
  if (!d) return;
  d->active = false;
  if (d->attachment) QObject::disconnect(d->attachment.get(), nullptr, d->attachment.get(), nullptr);
  if (d->nativePower) d->nativePower->stop();
  d->nativePower.reset();
  d->nativePowerStopped = false;
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
  d->sessionWatcher.reset();
  d->session1Owned = false;
}
}
