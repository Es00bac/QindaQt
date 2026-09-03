// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/app_shell/application_coordinator.h>
#include <qindaqt/app_shell/menu_export/application_menu_export.h>
#include <qindaqt/shell/global_menu/dbusmenu/dbusmenu_client.h>
#include <qindaqt/shell/global_menu/registrar/registrar_wire.h>

#include <QCloseEvent>
#include <QDBusObjectPath>
#include <QSignalSpy>
#include <QWindow>
#include <QtTest>

using namespace QindaQt;

namespace {

class FakeRegistrar final : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "com.canonical.AppMenu.Registrar")

public:
  int registerCount = 0;
  int unregisterCount = 0;
  quint32 windowId = 0;
  QDBusObjectPath objectPath;

public Q_SLOTS:
  Q_SCRIPTABLE void RegisterWindow(quint32 id, const QDBusObjectPath &path) {
    ++registerCount;
    windowId = id;
    objectPath = path;
  }

  Q_SCRIPTABLE void UnregisterWindow(quint32 id) {
    if (id == windowId) {
      ++unregisterCount;
    }
  }
};

class FakeIdentityPublisher final
    : public AppShell::MenuExport::WindowMenuIdentityPublisher {
public:
  explicit FakeIdentityPublisher(
      AppShell::MenuExport::WindowMenuIdentity identity)
      : m_identity(std::move(identity)) {}

  std::optional<AppShell::MenuExport::WindowMenuIdentity>
  publish(QWindow &, const QString &serviceName,
          const QString &objectPath) override {
    ++publishCount;
    observedService = serviceName;
    observedPath = objectPath;
    return m_identity;
  }

  void withdraw(QWindow &) override { ++withdrawCount; }

  int publishCount = 0;
  int withdrawCount = 0;
  QString observedService;
  QString observedPath;

private:
  AppShell::MenuExport::WindowMenuIdentity m_identity;
};

AppShell::ActionSpec action(bool enabled = true) {
  return {.id = QStringLiteral("file.open"),
          .menuId = QStringLiteral("file"),
          .menuLabel = QStringLiteral("File"),
          .label = QStringLiteral("Open"),
          .accessibleDescription = QStringLiteral("Open a document"),
          .shortcut = QKeySequence(QStringLiteral("Ctrl+O")),
          .menuOrder = 0,
          .order = 0,
          .enabled = enabled};
}

struct RegistrarFixture final {
  QString connectionName;
  QDBusConnection connection;
  FakeRegistrar registrar;

  explicit RegistrarFixture(QString name)
      : connectionName(std::move(name)),
        connection(QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                                 connectionName)) {}

  bool start() {
    return connection.isConnected() &&
           connection.registerObject(
               QString::fromLatin1(
                   Shell::GlobalMenu::Registrar::kRegistrarObjectPath),
               &registrar, QDBusConnection::ExportScriptableSlots) &&
           connection.registerService(QString::fromLatin1(
               Shell::GlobalMenu::Registrar::kRegistrarServiceName));
  }

  void stop() {
    connection.unregisterService(QString::fromLatin1(
        Shell::GlobalMenu::Registrar::kRegistrarServiceName));
    connection.unregisterObject(QString::fromLatin1(
        Shell::GlobalMenu::Registrar::kRegistrarObjectPath));
  }

  ~RegistrarFixture() {
    stop();
    QDBusConnection::disconnectFromBus(connectionName);
  }
};

QString firstActionId(const Shell::GlobalMenu::Protocol::MenuTree &tree) {
  return tree.items.isEmpty() || tree.items.constFirst().children.isEmpty()
             ? QString{}
             : tree.items.constFirst().children.constFirst().id;
}

} // namespace

class ApplicationMenuExportTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void exportsRealDbusMenuAndActivatesThroughCoordinatorOnce();
  void retriesOnRegistrarReplacementAndTearsDownOnClose();
  void publishesNativeWaylandAddressWithoutInventingWindowId();
  void productionIdentityRejectsOffscreenWindow();
};

void ApplicationMenuExportTest::
    exportsRealDbusMenuAndActivatesThroughCoordinatorOnce() {
  RegistrarFixture registrar(QStringLiteral("app-shell-export-registrar"));
  QVERIFY(registrar.start());
  auto providerBus = QDBusConnection::connectToBus(
      QDBusConnection::SessionBus, QStringLiteral("app-shell-export-provider"));
  auto consumerBus = QDBusConnection::connectToBus(
      QDBusConnection::SessionBus, QStringLiteral("app-shell-export-consumer"));
  QVERIFY(providerBus.isConnected());
  QVERIFY(consumerBus.isConnected());

  AppShell::ApplicationCoordinator coordinator;
  QVERIFY(coordinator.replaceActions({action()}).ok());
  QWindow window;
  auto publisher = std::make_unique<FakeIdentityPublisher>(
      AppShell::MenuExport::WindowMenuIdentity{
          .kind = AppShell::MenuExport::WindowMenuIdentityKind::XWindow,
          .registrarWindowId = 77});
  AppShell::MenuExport::ApplicationMenuExport composition(
      coordinator, window, providerBus, std::move(publisher));
  QVERIFY(composition.start());
  QTRY_VERIFY_WITH_TIMEOUT(composition.published(), 5'000);
  QCOMPARE(registrar.registrar.registerCount, 1);
  QCOMPARE(registrar.registrar.windowId, quint32{77});

  Shell::GlobalMenu::DbusMenu::DbusMenuClient client(
      consumerBus, providerBus.baseService(),
      QDBusObjectPath(QStringLiteral("/org/qindaqt/AppShell/Menu")),
      QUuid::createUuid(), 1'000);
  QVERIFY(client.start());
  QTRY_VERIFY_WITH_TIMEOUT(client.snapshot().complete, 5'000);
  const auto firstTree = client.snapshot().tree;
  QCOMPARE(firstTree.items.size(), 1);
  QCOMPARE(firstTree.items.constFirst().text, QStringLiteral("File"));
  QCOMPARE(firstTree.items.constFirst().children.constFirst().text,
           QStringLiteral("Open"));

  QSignalSpy activation(&coordinator,
                        &AppShell::ApplicationCoordinator::actionRequested);
  const qint32 transportId = firstActionId(firstTree).toInt();
  QVERIFY(transportId > 0);
  client.sendEvent(transportId, QStringLiteral("clicked"));
  QTRY_COMPARE_WITH_TIMEOUT(activation.size(), 1, 5'000);
  QTest::qWait(100);
  QCOMPARE(activation.size(), 1);

  QVERIFY(
      coordinator.setActionEnabled(QStringLiteral("file.open"), false).ok());
  QTRY_VERIFY_WITH_TIMEOUT(client.remoteRevision() > 1, 5'000);
  client.sendEvent(transportId, QStringLiteral("clicked"));
  QTest::qWait(100);
  QCOMPARE(activation.size(), 1);

  composition.stop();
  client.stop();
  QDBusConnection::disconnectFromBus(
      QStringLiteral("app-shell-export-consumer"));
  QDBusConnection::disconnectFromBus(
      QStringLiteral("app-shell-export-provider"));
}

void ApplicationMenuExportTest::
    retriesOnRegistrarReplacementAndTearsDownOnClose() {
  RegistrarFixture first(QStringLiteral("app-shell-owner-first"));
  QVERIFY(first.start());
  auto providerBus = QDBusConnection::connectToBus(
      QDBusConnection::SessionBus, QStringLiteral("app-shell-owner-provider"));
  QVERIFY(providerBus.isConnected());
  AppShell::ApplicationCoordinator coordinator;
  QVERIFY(coordinator.replaceActions({action()}).ok());
  QWindow window;
  auto publisher = std::make_unique<FakeIdentityPublisher>(
      AppShell::MenuExport::WindowMenuIdentity{
          .kind = AppShell::MenuExport::WindowMenuIdentityKind::XWindow,
          .registrarWindowId = 91});
  AppShell::MenuExport::ApplicationMenuExport composition(
      coordinator, window, providerBus, std::move(publisher));
  QVERIFY(composition.start());
  QTRY_VERIFY_WITH_TIMEOUT(composition.published(), 5'000);

  first.stop();
  QTRY_COMPARE_WITH_TIMEOUT(
      composition.status(),
      AppShell::MenuExport::MenuExportStatus::WaitingForRegistrar, 5'000);
  RegistrarFixture second(QStringLiteral("app-shell-owner-second"));
  QVERIFY(second.start());
  QTRY_VERIFY_WITH_TIMEOUT(composition.published(), 5'000);
  QCOMPARE(second.registrar.registerCount, 1);
  QCOMPARE(composition.registeredWindowId(), std::optional<quint32>{91});

  QCloseEvent closeEvent;
  QCoreApplication::sendEvent(&window, &closeEvent);
  QCOMPARE(composition.status(),
           AppShell::MenuExport::MenuExportStatus::Disabled);
  QTRY_COMPARE_WITH_TIMEOUT(second.registrar.unregisterCount, 1, 5'000);
  QDBusConnection::disconnectFromBus(
      QStringLiteral("app-shell-owner-provider"));
}

void ApplicationMenuExportTest::
    publishesNativeWaylandAddressWithoutInventingWindowId() {
  RegistrarFixture registrar(QStringLiteral("app-shell-wayland-registrar"));
  QVERIFY(registrar.start());
  auto providerBus = QDBusConnection::connectToBus(
      QDBusConnection::SessionBus,
      QStringLiteral("app-shell-wayland-provider"));
  AppShell::ApplicationCoordinator coordinator;
  QVERIFY(coordinator.replaceActions({action()}).ok());
  QWindow window;
  auto publisher = std::make_unique<FakeIdentityPublisher>(
      AppShell::MenuExport::WindowMenuIdentity{
          .kind =
              AppShell::MenuExport::WindowMenuIdentityKind::WaylandAnnouncement,
          .registrarWindowId = std::nullopt});
  auto *observedPublisher = publisher.get();
  AppShell::MenuExport::ApplicationMenuExport composition(
      coordinator, window, providerBus, std::move(publisher));
  QVERIFY(composition.start());
  QTRY_VERIFY_WITH_TIMEOUT(composition.published(), 5'000);
  QCOMPARE(composition.registeredWindowId(), std::nullopt);
  QCOMPARE(registrar.registrar.registerCount, 0);
  QCOMPARE(observedPublisher->observedService, providerBus.baseService());
  QCOMPARE(observedPublisher->observedPath,
           QStringLiteral("/org/qindaqt/AppShell/Menu"));
  composition.stop();
  QCOMPARE(observedPublisher->withdrawCount, 1);
  QDBusConnection::disconnectFromBus(
      QStringLiteral("app-shell-wayland-provider"));
}

void ApplicationMenuExportTest::productionIdentityRejectsOffscreenWindow() {
  RegistrarFixture registrar(QStringLiteral("app-shell-offscreen-registrar"));
  QVERIFY(registrar.start());
  auto providerBus = QDBusConnection::connectToBus(
      QDBusConnection::SessionBus,
      QStringLiteral("app-shell-offscreen-provider"));
  AppShell::ApplicationCoordinator coordinator;
  QVERIFY(coordinator.replaceActions({action()}).ok());
  QWindow window;
  auto composition = AppShell::MenuExport::ApplicationMenuExport::compose(
      coordinator, window, providerBus);
  QTRY_COMPARE_WITH_TIMEOUT(composition->failureCode(),
                            QStringLiteral("window-identity-unavailable"),
                            5'000);
  QVERIFY(!composition->published());
  QVERIFY(!composition->registeredWindowId());
  QCOMPARE(registrar.registrar.registerCount, 0);
  QDBusConnection::disconnectFromBus(
      QStringLiteral("app-shell-offscreen-provider"));
}

QTEST_MAIN(ApplicationMenuExportTest)

#include "tst_application_menu_export.moc"
