// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/app_shell/menu_export/application_menu_export.h>

#include <qindaqt/app_shell/application_coordinator.h>
#include <qindaqt/shell/global_menu/dbusmenu/dbusmenu_server.h>
#include <qindaqt/shell/global_menu/registrar/registrar_wire.h>

#include "coordinator_menu_projection_p.h"
#include "registration_completion_p.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QEvent>
#include <QPlatformSurfaceEvent>
#include <QPointer>
#include <QTimer>
#include <QWindow>

#include <utility>

namespace QindaQt::AppShell::MenuExport {
namespace {

namespace DbusMenu = Shell::GlobalMenu::DbusMenu;
namespace Protocol = Shell::GlobalMenu::Protocol;
namespace Registrar = Shell::GlobalMenu::Registrar;

constexpr auto kMenuObjectPath = "/org/qindaqt/AppShell/Menu";
constexpr auto kBusDaemonService = "org.freedesktop.DBus";
constexpr auto kBusDaemonPath = "/org/freedesktop/DBus";
constexpr auto kBusDaemonInterface = "org.freedesktop.DBus";

} // namespace

class ApplicationMenuExport::Private final {
public:
  Private(ApplicationMenuExport &owner, ApplicationCoordinator &coordinatorRef,
          QWindow &windowRef, QDBusConnection bus,
          std::unique_ptr<WindowMenuIdentityPublisher> publisher)
      : q(owner), coordinator(coordinatorRef), window(&windowRef),
        sessionBus(std::move(bus)), identityPublisher(std::move(publisher)),
        projection(coordinatorRef) {
    QObject::connect(&menuServer, &DbusMenu::DbusMenuServer::actionActivated,
                     &q, [this](const QString &actionId) {
                       // AGENT-CONTRACT: the accepted transport server only
                       // identifies the action. AppShell rechecks current
                       // known/enabled consent exactly as its local menu does.
                       (void)coordinator.activateAction(actionId);
                     });
  }

  void setStatus(MenuExportStatus next, QString failure = {}) {
    if (status == next && failureCode == failure) {
      return;
    }
    status = next;
    failureCode = std::move(failure);
    if (status != MenuExportStatus::Published) {
      setHosted(false);
    }
    Q_EMIT q.statusChanged();
    if (status == MenuExportStatus::Published) {
      queryHostedMenu();
    }
  }

  void setHosted(bool next) {
    if (hosted == next) {
      return;
    }
    hosted = next;
    Q_EMIT q.localMenuVisibleChanged();
  }

  void queryHostedMenu() {
    if (!started || status != MenuExportStatus::Published
        || registrarOwner.isEmpty()) {
      setHosted(false);
      return;
    }
    const quint64 requestSerial = ++hostSerial;
    QDBusMessage message = QDBusMessage::createMethodCall(
        registrarOwner, QString::fromLatin1(Registrar::kRegistrarObjectPath),
        QString::fromLatin1(Registrar::kRegistrarInterface),
        QStringLiteral("IsMenuHosted"));
    message.setArguments(
        {sessionBus.baseService(),
         QVariant::fromValue(QDBusObjectPath(QString::fromLatin1(kMenuObjectPath)))});
    auto *watcher =
        new QDBusPendingCallWatcher(sessionBus.asyncCall(message, 2'000), &q);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, &q,
                     [this, watcher, requestSerial] {
                       const QDBusPendingReply<bool> reply = *watcher;
                       watcher->deleteLater();
                       if (!started || requestSerial != hostSerial
                           || status != MenuExportStatus::Published) {
                         return;
                       }
                       setHosted(reply.isValid() && reply.value());
                     });
  }

  void refreshMenu() {
    if (!menuServer.publish(projection.snapshot())) {
      setStatus(MenuExportStatus::Disabled,
                QStringLiteral("menu-snapshot-rejected"));
    }
  }

  void requestInitialOwner() {
    QDBusMessage message =
        QDBusMessage::createMethodCall(QString::fromLatin1(kBusDaemonService),
                                       QString::fromLatin1(kBusDaemonPath),
                                       QString::fromLatin1(kBusDaemonInterface),
                                       QStringLiteral("GetNameOwner"));
    message.setArguments(
        {QString::fromLatin1(Registrar::kRegistrarServiceName)});
    const quint64 requestSerial = ++serial;
    auto *watcher =
        new QDBusPendingCallWatcher(sessionBus.asyncCall(message, 2'000), &q);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, &q,
                     [this, watcher, requestSerial] {
                       const QDBusPendingReply<QString> reply = *watcher;
                       watcher->deleteLater();
                       if (!started || requestSerial != serial) {
                         return;
                       }
                       handleRegistrarOwner(reply.isValid() ? reply.value()
                                                            : QString{});
                     });
  }

  void handleRegistrarOwner(const QString &newOwner) {
    if (!started || newOwner == registrarOwner) {
      return;
    }
    if (!registrarOwner.isEmpty()) {
      sessionBus.disconnect(
          registrarOwner, QString::fromLatin1(Registrar::kRegistrarObjectPath),
          QString::fromLatin1(Registrar::kRegistrarInterface),
          QStringLiteral("MenuHostedChanged"), &q,
          SLOT(handleMenuHostedChanged(QString,QDBusObjectPath,bool)));
    }
    retirePublishedIdentity();
    registrarOwner = newOwner;
    ++hostSerial;
    setHosted(false);
    ++serial;
    if (registrarOwner.isEmpty()) {
      setStatus(MenuExportStatus::WaitingForRegistrar,
                QStringLiteral("registrar-unavailable"));
      return;
    }
    sessionBus.connect(
        registrarOwner, QString::fromLatin1(Registrar::kRegistrarObjectPath),
        QString::fromLatin1(Registrar::kRegistrarInterface),
        QStringLiteral("MenuHostedChanged"), &q,
        SLOT(handleMenuHostedChanged(QString,QDBusObjectPath,bool)));
    publishIdentity();
  }

  void publishIdentity() {
    if (window.isNull()) {
      setStatus(MenuExportStatus::Disabled, QStringLiteral("window-destroyed"));
      return;
    }
    if (surfaceDestroyed) {
      // AGENT-GUARD: a registrar-owner change must never publish against the
      // dead pre-recreation window identity; only SurfaceCreated may lift
      // this by obtaining a fresh identity from the publisher.
      setStatus(MenuExportStatus::WaitingForRegistrar,
                QStringLiteral("window-surface-destroyed"));
      return;
    }
    identity = identityPublisher->publish(*window, sessionBus.baseService(),
                                          QString::fromLatin1(kMenuObjectPath));
    if (!identity) {
      setStatus(MenuExportStatus::WaitingForRegistrar,
                QStringLiteral("window-identity-unavailable"));
      return;
    }
    if (identity->kind == WindowMenuIdentityKind::WaylandAnnouncement) {
      setStatus(MenuExportStatus::Published);
      return;
    }
    if (!identity->registrarWindowId || *identity->registrarWindowId == 0) {
      identityPublisher->withdraw(*window);
      identity.reset();
      setStatus(MenuExportStatus::Disabled,
                QStringLiteral("invalid-window-identity"));
      return;
    }
    setStatus(MenuExportStatus::Registering);
    const quint64 requestSerial = ++serial;
    QDBusMessage message = QDBusMessage::createMethodCall(
        registrarOwner, QString::fromLatin1(Registrar::kRegistrarObjectPath),
        QString::fromLatin1(Registrar::kRegistrarInterface),
        QStringLiteral("RegisterWindow"));
    message.setArguments({QVariant::fromValue(*identity->registrarWindowId),
                          QVariant::fromValue(QDBusObjectPath(
                              QString::fromLatin1(kMenuObjectPath)))});
    // AGENT-GUARD: the registrar may have accepted this window id before its
    // reply ever arrives, so the attempt is registrar state from this moment.
    // Every withdrawal must compensate pendingRegistration (or the
    // reply-confirmed registeredWindowId) exactly once; dropping this record
    // leaks a live registration across surface recreation.
    pendingRegistration = PendingRegistration{*identity->registrarWindowId,
                                              requestSerial, registrarOwner};
    auto *watcher =
        new QDBusPendingCallWatcher(sessionBus.asyncCall(message, 2'000), &q);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, &q,
                     [this, watcher, requestSerial] {
                       const QDBusMessage reply = watcher->reply();
                       watcher->deleteLater();
                       if (!started || requestSerial != serial ||
                           !pendingRegistration ||
                           pendingRegistration->requestSerial !=
                               requestSerial) {
                         // AGENT-GUARD: a superseded attempt's reply is noise.
                         // The withdrawal that advanced the serial already
                         // compensated the attempted id; acting here would
                         // either resurrect it or double the compensation.
                         return;
                       }
                       switch (classifyRegistrationCompletion(reply)) {
                       case RegistrationCompletion::Confirmed:
                         registeredWindowId = pendingRegistration->windowId;
                         pendingRegistration.reset();
                         setStatus(MenuExportStatus::Published);
                         break;
                       case RegistrationCompletion::Refused:
                         // Only a method-level error is authoritative refusal.
                         // Local transport errors do not prove that the owner
                         // failed to mutate state.
                         pendingRegistration.reset();
                         setStatus(
                             MenuExportStatus::WaitingForRegistrar,
                             QStringLiteral("registrar-registration-failed"));
                         break;
                       case RegistrationCompletion::Uncertain:
                         // AGENT-GUARD: timeout, disconnect, and malformed
                         // success replies retain the attempted id. A later
                         // withdrawal must compensate it exactly once because
                         // none of those outcomes proves registrar refusal.
                         setStatus(
                             MenuExportStatus::WaitingForRegistrar,
                             QStringLiteral("registrar-registration-uncertain"));
                         break;
                       }
                     });
  }

  void retirePublishedIdentity() {
    // AGENT-GUARD: compensate the id the registrar may already hold whether
    // or not its RegisterWindow reply has arrived. Clearing both records in
    // the same turn makes the compensation exactly-once per attempt: a second
    // retirement (stop after surface destruction) finds neither and sends
    // nothing.
    const std::optional<quint32> attemptedId =
        registeredWindowId
            ? registeredWindowId
            : (pendingRegistration
                   ? std::optional<quint32>(pendingRegistration->windowId)
                   : std::nullopt);
    const QString attemptedOwner =
        pendingRegistration ? pendingRegistration->registrarOwner
                            : registrarOwner;
    if (attemptedId && !attemptedOwner.isEmpty()) {
      QDBusMessage message = QDBusMessage::createMethodCall(
          attemptedOwner, QString::fromLatin1(Registrar::kRegistrarObjectPath),
          QString::fromLatin1(Registrar::kRegistrarInterface),
          QStringLiteral("UnregisterWindow"));
      message.setArguments({QVariant::fromValue(*attemptedId)});
      sessionBus.asyncCall(message, 2'000);
    }
    registeredWindowId.reset();
    pendingRegistration.reset();
    if (identity && !window.isNull()) {
      identityPublisher->withdraw(*window);
    }
    identity.reset();
  }

  void handleSurfaceAboutToBeDestroyed() {
    if (!started) {
      return;
    }
    // AGENT-GUARD: the native window identity dies with its surface. Retire
    // the registrar association synchronously here — the Wayland withdrawal
    // needs the still-live surface — and never reuse the old identity after.
    surfaceDestroyed = true;
    ++serial;
    retirePublishedIdentity();
    setStatus(MenuExportStatus::WaitingForRegistrar,
              QStringLiteral("window-surface-destroyed"));
  }

  void handleSurfaceCreated() {
    if (!started) {
      return;
    }
    surfaceDestroyed = false;
    // AGENT-NOTE: SurfaceCreated can be delivered synchronously inside
    // publishIdentity itself (reading an X11 winId creates the surface), so
    // republishing is deferred to a stable turn and every guard is rechecked
    // there instead of recursing.
    QMetaObject::invokeMethod(
        &q,
        [this] {
          if (!started || surfaceDestroyed || identity.has_value() ||
              registrarOwner.isEmpty()) {
            return;
          }
          publishIdentity();
        },
        Qt::QueuedConnection);
  }

  ApplicationMenuExport &q;
  ApplicationCoordinator &coordinator;
  // AGENT-GUARD: QML test and shutdown paths can destroy the root window
  // before this composition; weak tracking keeps stop() from dereferencing it.
  QPointer<QWindow> window;
  QDBusConnection sessionBus;
  std::unique_ptr<WindowMenuIdentityPublisher> identityPublisher;
  CoordinatorMenuProjection projection;
  DbusMenu::DbusMenuServer menuServer;
  QDBusServiceWatcher *registrarWatcher = nullptr;
  QMetaObject::Connection menusConnection;
  QString registrarOwner;
  QString failureCode;
  std::optional<WindowMenuIdentity> identity;
  std::optional<quint32> registeredWindowId;
  // One RegisterWindow attempt retained from send until confirmation,
  // explicit registrar refusal, or exactly-once compensating withdrawal.
  struct PendingRegistration {
    quint32 windowId;
    quint64 requestSerial;
    QString registrarOwner;
  };
  std::optional<PendingRegistration> pendingRegistration;
  quint64 serial = 0;
  quint64 closeCheckSerial = 0;
  MenuExportStatus status = MenuExportStatus::Disabled;
  bool started = false;
  bool objectRegistered = false;
  // True only between SurfaceAboutToBeDestroyed and SurfaceCreated; an uncreated
  // test window is never marked, so injected-identity rows keep publishing.
  bool surfaceDestroyed = false;
  bool hosted = false;
  quint64 hostSerial = 0;
};

ApplicationMenuExport::ApplicationMenuExport(
    ApplicationCoordinator &coordinator, QWindow &window,
    QDBusConnection sessionBus,
    std::unique_ptr<WindowMenuIdentityPublisher> identityPublisher,
    QObject *parent)
    : QObject(parent),
      d(std::make_unique<Private>(*this, coordinator, window,
                                  std::move(sessionBus),
                                  std::move(identityPublisher))) {}

ApplicationMenuExport::~ApplicationMenuExport() { stop(); }

std::unique_ptr<ApplicationMenuExport>
ApplicationMenuExport::compose(ApplicationCoordinator &coordinator,
                               QWindow &window, QDBusConnection sessionBus) {
  auto composition = std::make_unique<ApplicationMenuExport>(
      coordinator, window, std::move(sessionBus),
      createQtWindowMenuIdentityPublisher());
  (void)composition->start();
  return composition;
}

bool ApplicationMenuExport::start() {
  if (d->started) {
    return d->status != MenuExportStatus::Disabled;
  }
  if (!d->sessionBus.isConnected() || d->sessionBus.baseService().isEmpty() ||
      d->identityPublisher == nullptr) {
    d->setStatus(MenuExportStatus::Disabled,
                 QStringLiteral("session-bus-unavailable"));
    return false;
  }
  if (!d->sessionBus.registerObject(
          QString::fromLatin1(kMenuObjectPath), &d->menuServer,
          QDBusConnection::ExportScriptableSlots |
              QDBusConnection::ExportScriptableSignals |
              QDBusConnection::ExportScriptableProperties)) {
    d->setStatus(MenuExportStatus::Disabled,
                 QStringLiteral("menu-object-registration-failed"));
    return false;
  }
  d->objectRegistered = true;
  d->started = true;
  d->refreshMenu();
  d->menusConnection =
      connect(&d->coordinator, &ApplicationCoordinator::menusChanged, this,
              [this] { d->refreshMenu(); });
  d->window->installEventFilter(this);
  connect(d->window, &QObject::destroyed, this, [this] { stop(); });
  d->registrarWatcher = new QDBusServiceWatcher(
      QString::fromLatin1(Registrar::kRegistrarServiceName), d->sessionBus,
      QDBusServiceWatcher::WatchForOwnerChange, this);
  connect(d->registrarWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
          [this](const QString &, const QString &, const QString &newOwner) {
            d->handleRegistrarOwner(newOwner);
          });
  d->setStatus(MenuExportStatus::WaitingForRegistrar,
               QStringLiteral("registrar-unavailable"));
  d->requestInitialOwner();
  return true;
}

void ApplicationMenuExport::stop() {
  if (!d->started && !d->objectRegistered) {
    return;
  }
  d->started = false;
  ++d->hostSerial;
  d->setHosted(false);
  ++d->serial;
  d->surfaceDestroyed = false;
  if (!d->window.isNull()) {
    d->window->removeEventFilter(this);
  }
  disconnect(d->menusConnection);
  d->retirePublishedIdentity();
  if (!d->registrarOwner.isEmpty()) {
    d->sessionBus.disconnect(
        d->registrarOwner,
        QString::fromLatin1(Registrar::kRegistrarObjectPath),
        QString::fromLatin1(Registrar::kRegistrarInterface),
        QStringLiteral("MenuHostedChanged"), this,
        SLOT(handleMenuHostedChanged(QString,QDBusObjectPath,bool)));
  }
  d->registrarOwner.clear();
  if (d->objectRegistered) {
    d->sessionBus.unregisterObject(QString::fromLatin1(kMenuObjectPath));
    d->objectRegistered = false;
  }
  d->setStatus(MenuExportStatus::Disabled);
}

MenuExportStatus ApplicationMenuExport::status() const noexcept {
  return d->status;
}

bool ApplicationMenuExport::published() const noexcept {
  return d->status == MenuExportStatus::Published;
}

bool ApplicationMenuExport::localMenuVisible() const noexcept {
  return !d->hosted;
}

void ApplicationMenuExport::handleMenuHostedChanged(
    const QString &providerUniqueName, const QDBusObjectPath &menuObjectPath,
    bool hosted) {
  if (!d->started || d->status != MenuExportStatus::Published
      || providerUniqueName != d->sessionBus.baseService()
      || menuObjectPath.path() != QString::fromLatin1(kMenuObjectPath)) {
    return;
  }
  ++d->hostSerial;
  if (!hosted) {
    d->setHosted(false);
  }
  // The signal carries endpoint identity but not registrar-owner lineage.
  // Re-read through the current exact owner before settling; a queued signal
  // from a replaced registrar must neither suppress nor expose the menu.
  d->queryHostedMenu();
}

std::optional<quint32>
ApplicationMenuExport::registeredWindowId() const noexcept {
  return d->registeredWindowId;
}

QString ApplicationMenuExport::failureCode() const { return d->failureCode; }

bool ApplicationMenuExport::eventFilter(QObject *watched, QEvent *event) {
  if (watched == d->window) {
    if (event->type() == QEvent::PlatformSurface) {
      // AGENT-GUARD: Wayland windows routinely destroy and recreate their
      // native surface without destroying the QWindow. Each recreation must
      // swap the registrar identity exactly; keeping the old registration
      // leaves the shell joined to a dead window.
      const auto *surfaceEvent =
          static_cast<const QPlatformSurfaceEvent *>(event);
      if (surfaceEvent->surfaceEventType() ==
          QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
        d->handleSurfaceAboutToBeDestroyed();
      } else if (surfaceEvent->surfaceEventType() ==
                 QPlatformSurfaceEvent::SurfaceCreated) {
        d->handleSurfaceCreated();
      }
    } else if (event->type() == QEvent::Close) {
      const quint64 closeSerial = ++d->closeCheckSerial;
      QTimer::singleShot(0, this, [this, closeSerial] {
        if (!d->started || closeSerial != d->closeCheckSerial ||
            d->window.isNull()) {
          return;
        }
        // AGENT-GUARD: event filters run before ApplicationShell's consent
        // handler. Withdraw only after the event turn proves the surface really
        // closed; a rejected close leaves the live menu association intact.
        if (!d->window->isVisible()) {
          stop();
        }
      });
    }
  }
  return QObject::eventFilter(watched, event);
}

} // namespace QindaQt::AppShell::MenuExport
