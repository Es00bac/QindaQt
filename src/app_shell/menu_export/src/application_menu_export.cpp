// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/app_shell/menu_export/application_menu_export.h>

#include <qindaqt/app_shell/application_coordinator.h>
#include <qindaqt/shell/global_menu/dbusmenu/dbusmenu_server.h>
#include <qindaqt/shell/global_menu/registrar/registrar_wire.h>

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QEvent>
#include <QPointer>
#include <QTimer>
#include <QWindow>

#include <algorithm>
#include <tuple>
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

class CoordinatorMenuProjection final {
public:
  explicit CoordinatorMenuProjection(const ApplicationCoordinator &coordinator)
      : m_coordinator(coordinator) {}

  [[nodiscard]] Protocol::MenuTree snapshot() const {
    QList<ActionSpec> actions = m_coordinator.actionRegistry().actions();
    std::sort(
        actions.begin(), actions.end(),
        [](const ActionSpec &left, const ActionSpec &right) {
          return std::tie(left.menuOrder, left.menuId, left.order, left.id) <
                 std::tie(right.menuOrder, right.menuId, right.order, right.id);
        });
    Protocol::MenuTree tree;
    for (qsizetype index = 0; index < actions.size();) {
      const QString menuId = actions.at(index).menuId;
      Protocol::MenuItem menu{
          .id = QStringLiteral("menu:") + menuId,
          .kind = Protocol::MenuItemKind::Submenu,
          .text = actions.at(index).menuLabel};
      while (index < actions.size() && actions.at(index).menuId == menuId) {
        const ActionSpec &action = actions.at(index++);
        menu.children.append(Protocol::MenuItem{
            .id = action.id,
            .kind = Protocol::MenuItemKind::Action,
            .text = action.label,
            .mnemonicIndex = -1,
            .shortcutText =
                action.shortcut.toString(QKeySequence::PortableText),
            .enabled = action.enabled,
            .visible = true,
            .checkable = action.checkable,
            .checked = action.checked});
      }
      tree.items.append(std::move(menu));
    }
    return tree;
  }

private:
  const ApplicationCoordinator &m_coordinator;
};

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
    Q_EMIT q.statusChanged();
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
    retirePublishedIdentity();
    registrarOwner = newOwner;
    ++serial;
    if (registrarOwner.isEmpty()) {
      setStatus(MenuExportStatus::WaitingForRegistrar,
                QStringLiteral("registrar-unavailable"));
      return;
    }
    publishIdentity();
  }

  void publishIdentity() {
    if (window.isNull()) {
      setStatus(MenuExportStatus::Disabled, QStringLiteral("window-destroyed"));
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
    auto *watcher =
        new QDBusPendingCallWatcher(sessionBus.asyncCall(message, 2'000), &q);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, &q,
                     [this, watcher, requestSerial] {
                       const QDBusMessage reply = watcher->reply();
                       watcher->deleteLater();
                       if (!started || requestSerial != serial) {
                         return;
                       }
                       if (reply.type() == QDBusMessage::ReplyMessage) {
                         registeredWindowId = identity->registrarWindowId;
                         setStatus(MenuExportStatus::Published);
                       } else {
                         setStatus(
                             MenuExportStatus::WaitingForRegistrar,
                             QStringLiteral("registrar-registration-failed"));
                       }
                     });
  }

  void retirePublishedIdentity() {
    if (registeredWindowId && !registrarOwner.isEmpty()) {
      QDBusMessage message = QDBusMessage::createMethodCall(
          registrarOwner, QString::fromLatin1(Registrar::kRegistrarObjectPath),
          QString::fromLatin1(Registrar::kRegistrarInterface),
          QStringLiteral("UnregisterWindow"));
      message.setArguments({QVariant::fromValue(*registeredWindowId)});
      sessionBus.asyncCall(message, 2'000);
    }
    registeredWindowId.reset();
    if (identity && !window.isNull()) {
      identityPublisher->withdraw(*window);
    }
    identity.reset();
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
  quint64 serial = 0;
  quint64 closeCheckSerial = 0;
  MenuExportStatus status = MenuExportStatus::Disabled;
  bool started = false;
  bool objectRegistered = false;
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
  ++d->serial;
  if (!d->window.isNull()) {
    d->window->removeEventFilter(this);
  }
  disconnect(d->menusConnection);
  d->retirePublishedIdentity();
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

std::optional<quint32>
ApplicationMenuExport::registeredWindowId() const noexcept {
  return d->registeredWindowId;
}

QString ApplicationMenuExport::failureCode() const { return d->failureCode; }

bool ApplicationMenuExport::eventFilter(QObject *watched, QEvent *event) {
  if (watched == d->window && event->type() == QEvent::Close) {
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
  return QObject::eventFilter(watched, event);
}

} // namespace QindaQt::AppShell::MenuExport
