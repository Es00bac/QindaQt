// SPDX-License-Identifier: GPL-3.0-or-later
// Surface-recreation lifecycle rows for the first-party menu export. Each row
// destroys and recreates a real QWindow native surface without destroying the
// QWindow, which is the Wayland window-management shape the export must
// survive by swapping its registrar identity exactly.
#include <qindaqt/app_shell/application_coordinator.h>
#include <qindaqt/app_shell/menu_export/application_menu_export.h>
#include <qindaqt/app_shell/menu_export/first_party_composition.h>
#include <qindaqt/shell/global_menu/dbusmenu/dbusmenu_client.h>
#include <qindaqt/shell/global_menu/registrar/registrar_wire.h>

#include <QDBusObjectPath>
#include <QSignalSpy>
#include <QWindow>
#include <QtTest>

#include <memory>

using namespace QindaQt;

namespace {

class RecordingRegistrar final : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "com.canonical.AppMenu.Registrar")

public:
  QList<quint32> registerCalls;
  QList<quint32> unregisterCalls;

public Q_SLOTS:
  Q_SCRIPTABLE void RegisterWindow(quint32 id, const QDBusObjectPath &path) {
    (void)path;
    registerCalls.append(id);
  }

  Q_SCRIPTABLE void UnregisterWindow(quint32 id) {
    unregisterCalls.append(id);
  }
};

// Publishes 71 for the original surface and 72 for any recreated surface, so a
// row can prove the export republished instead of reusing the stale identity.
class RotatingIdentityPublisher final
    : public AppShell::MenuExport::WindowMenuIdentityPublisher {
public:
  std::optional<AppShell::MenuExport::WindowMenuIdentity>
  publish(QWindow &, const QString &, const QString &) override {
    ++publishCount;
    return AppShell::MenuExport::WindowMenuIdentity{
        .kind = AppShell::MenuExport::WindowMenuIdentityKind::XWindow,
        .registrarWindowId = publishCount == 1 ? 71U : 72U};
  }

  void withdraw(QWindow &) override { ++withdrawCount; }

  int publishCount = 0;
  int withdrawCount = 0;
};

struct RegistrarFixture final {
  QString connectionName;
  QDBusConnection connection;
  RecordingRegistrar registrar;

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

  // Releases only the well-known name while keeping the registrar object
  // registered on this connection, so the export's asynchronous
  // UnregisterWindow for the retired owner remains observable after loss.
  void releaseName() {
    connection.unregisterService(QString::fromLatin1(
        Shell::GlobalMenu::Registrar::kRegistrarServiceName));
  }

  ~RegistrarFixture() {
    stop();
    QDBusConnection::disconnectFromBus(connectionName);
  }
};

AppShell::ActionSpec action() {
  return {.id = QStringLiteral("file.open"),
          .menuId = QStringLiteral("file"),
          .menuLabel = QStringLiteral("File"),
          .label = QStringLiteral("Open"),
          .accessibleDescription = QStringLiteral("Open a document"),
          .shortcut = QKeySequence(QStringLiteral("Ctrl+O")),
          .menuOrder = 0,
          .order = 0,
          .enabled = true};
}

QString firstActionId(const Shell::GlobalMenu::Protocol::MenuTree &tree) {
  return tree.items.isEmpty() || tree.items.constFirst().children.isEmpty()
             ? QString{}
             : tree.items.constFirst().children.constFirst().id;
}

} // namespace

class ApplicationMenuExportSurfaceTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void recreatedSurfaceSwapsExactRegistrarIdentity();
  void surfaceRecreationWithoutRegistrarStaysFailClosedThenRebinds();
  void composedFirstPartyExportReRegistersRecreatedSurface();
};

void ApplicationMenuExportSurfaceTest::
    recreatedSurfaceSwapsExactRegistrarIdentity() {
  RegistrarFixture registrar(QStringLiteral("menu-export-surface-registrar"));
  QVERIFY(registrar.start());
  auto providerBus = QDBusConnection::connectToBus(
      QDBusConnection::SessionBus,
      QStringLiteral("menu-export-surface-provider"));
  auto consumerBus = QDBusConnection::connectToBus(
      QDBusConnection::SessionBus,
      QStringLiteral("menu-export-surface-consumer"));
  QVERIFY(providerBus.isConnected());
  QVERIFY(consumerBus.isConnected());

  AppShell::ApplicationCoordinator coordinator;
  QVERIFY(coordinator.replaceActions({action()}).ok());
  QWindow window;
  window.create();
  auto publisher = std::make_unique<RotatingIdentityPublisher>();
  auto *probe = publisher.get();
  AppShell::MenuExport::ApplicationMenuExport composition(
      coordinator, window, providerBus, std::move(publisher));
  QVERIFY(composition.start());
  QTRY_VERIFY_WITH_TIMEOUT(composition.published(), 5'000);
  QCOMPARE(probe->publishCount, 1);
  QCOMPARE(registrar.registrar.registerCalls, QList<quint32>{71});
  QCOMPARE(composition.registeredWindowId(), std::optional<quint32>{71});

  // The stale-identity defect: destroying only the native surface must
  // withdraw identity 71 exactly once and never keep it registered.
  window.destroy();
  QCOMPARE(composition.status(),
           AppShell::MenuExport::MenuExportStatus::WaitingForRegistrar);
  QCOMPARE(composition.failureCode(),
           QStringLiteral("window-surface-destroyed"));
  QVERIFY(!composition.published());
  QCOMPARE(probe->withdrawCount, 1);
  QTRY_COMPARE_WITH_TIMEOUT(registrar.registrar.unregisterCalls,
                            QList<quint32>{71}, 5'000);
  QTest::qWait(150);
  QCOMPARE(probe->publishCount, 1);
  QCOMPARE(registrar.registrar.registerCalls, QList<quint32>{71});
  QCOMPARE(registrar.registrar.unregisterCalls, QList<quint32>{71});

  // Recreating the surface must publish the freshly obtained identity 72
  // exactly once and re-register it with the same registrar owner.
  window.create();
  QTRY_VERIFY_WITH_TIMEOUT(composition.published(), 5'000);
  QCOMPARE(probe->publishCount, 2);
  QCOMPARE(probe->withdrawCount, 1);
  QCOMPARE(registrar.registrar.registerCalls, (QList<quint32>{71, 72}));
  QCOMPARE(composition.registeredWindowId(), std::optional<quint32>{72});

  // The recreated registration must still serve the live tree with a coherent
  // revision: a fresh consumer join reads it, one activation crosses into the
  // coordinator, and a catalog change re-advances the exposed revision.
  Shell::GlobalMenu::DbusMenu::DbusMenuClient client(
      consumerBus, providerBus.baseService(),
      QDBusObjectPath(QStringLiteral("/org/qindaqt/AppShell/Menu")),
      QUuid::createUuid(), 1'000);
  QVERIFY(client.start());
  QTRY_VERIFY_WITH_TIMEOUT(client.snapshot().complete, 5'000);
  const auto tree = client.snapshot().tree;
  QCOMPARE(tree.items.size(), 1);
  QCOMPARE(tree.items.constFirst().text, QStringLiteral("File"));
  QCOMPARE(tree.items.constFirst().children.constFirst().text,
           QStringLiteral("Open"));

  QSignalSpy activation(&coordinator,
                        &AppShell::ApplicationCoordinator::actionRequested);
  const qint32 transportId = firstActionId(tree).toInt();
  QVERIFY(transportId > 0);
  client.sendEvent(transportId, QStringLiteral("clicked"));
  QTRY_COMPARE_WITH_TIMEOUT(activation.size(), 1, 5'000);

  const quint32 revisionAtRecreation = client.remoteRevision();
  QVERIFY(
      coordinator.setActionEnabled(QStringLiteral("file.open"), false).ok());
  QTRY_VERIFY_WITH_TIMEOUT(client.remoteRevision() > revisionAtRecreation,
                           5'000);
  client.sendEvent(transportId, QStringLiteral("clicked"));
  QTest::qWait(100);
  QCOMPARE(activation.size(), 1);
  QCOMPARE(registrar.registrar.registerCalls, (QList<quint32>{71, 72}));

  composition.stop();
  client.stop();
  QDBusConnection::disconnectFromBus(
      QStringLiteral("menu-export-surface-consumer"));
  QDBusConnection::disconnectFromBus(
      QStringLiteral("menu-export-surface-provider"));
}

void ApplicationMenuExportSurfaceTest::
    surfaceRecreationWithoutRegistrarStaysFailClosedThenRebinds() {
  RegistrarFixture first(QStringLiteral("menu-export-surface-absent-first"));
  QVERIFY(first.start());
  auto providerBus = QDBusConnection::connectToBus(
      QDBusConnection::SessionBus,
      QStringLiteral("menu-export-surface-absent-provider"));
  QVERIFY(providerBus.isConnected());
  AppShell::ApplicationCoordinator coordinator;
  QVERIFY(coordinator.replaceActions({action()}).ok());
  QWindow window;
  window.create();
  auto publisher = std::make_unique<RotatingIdentityPublisher>();
  auto *probe = publisher.get();
  AppShell::MenuExport::ApplicationMenuExport composition(
      coordinator, window, providerBus, std::move(publisher));
  QVERIFY(composition.start());
  QTRY_VERIFY_WITH_TIMEOUT(composition.published(), 5'000);

  first.releaseName();
  QTRY_COMPARE_WITH_TIMEOUT(
      composition.status(),
      AppShell::MenuExport::MenuExportStatus::WaitingForRegistrar, 5'000);
  QTRY_COMPARE_WITH_TIMEOUT(probe->withdrawCount, 1, 5'000);
  QTRY_COMPARE_WITH_TIMEOUT(first.registrar.unregisterCalls,
                            QList<quint32>{71}, 5'000);

  // Negative control: recreation while no registrar owner exists must fail
  // closed — no publish, no crash, still waiting — and the export must
  // republish the fresh identity once an owner returns.
  window.destroy();
  window.create();
  QTest::qWait(300);
  QCOMPARE(probe->publishCount, 1);
  QCOMPARE(composition.status(),
           AppShell::MenuExport::MenuExportStatus::WaitingForRegistrar);
  QVERIFY(!composition.published());
  QVERIFY(!composition.registeredWindowId().has_value());

  RegistrarFixture second(QStringLiteral("menu-export-surface-absent-second"));
  QVERIFY(second.start());
  QTRY_VERIFY_WITH_TIMEOUT(composition.published(), 5'000);
  QCOMPARE(probe->publishCount, 2);
  QCOMPARE(second.registrar.registerCalls, QList<quint32>{72});
  QCOMPARE(second.registrar.unregisterCalls, QList<quint32>{});
  QCOMPARE(composition.registeredWindowId(), std::optional<quint32>{72});

  composition.stop();
  QDBusConnection::disconnectFromBus(
      QStringLiteral("menu-export-surface-absent-provider"));
}

void ApplicationMenuExportSurfaceTest::
    composedFirstPartyExportReRegistersRecreatedSurface() {
  // Per-app variant: the composed first-party entry that Terminal and Text
  // Editor call differs only through its fixed-id test seam, so this row
  // proves the composed path also withdraws and re-registers on recreation.
  RegistrarFixture registrar(QStringLiteral("menu-export-surface-composed"));
  QVERIFY(registrar.start());
  auto providerBus = QDBusConnection::connectToBus(
      QDBusConnection::SessionBus,
      QStringLiteral("menu-export-surface-composed-provider"));
  auto consumerBus = QDBusConnection::connectToBus(
      QDBusConnection::SessionBus,
      QStringLiteral("menu-export-surface-composed-consumer"));
  QVERIFY(providerBus.isConnected());
  QVERIFY(consumerBus.isConnected());

  AppShell::ApplicationCoordinator coordinator;
  QVERIFY(coordinator.replaceActions({action()}).ok());
  QWindow window;
  window.create();
  qputenv("QINDAQT_TEST_APPMENU_WINDOW_ID", QByteArray("83"));
  auto composition = AppShell::MenuExport::composeFirstPartyMenuExport(
      coordinator, window, providerBus);
  qunsetenv("QINDAQT_TEST_APPMENU_WINDOW_ID");
  QVERIFY(composition != nullptr);
  QTRY_COMPARE_WITH_TIMEOUT(registrar.registrar.registerCalls,
                            QList<quint32>{83}, 5'000);

  window.destroy();
  QTRY_COMPARE_WITH_TIMEOUT(registrar.registrar.unregisterCalls,
                            QList<quint32>{83}, 5'000);
  QTest::qWait(150);
  QCOMPARE(registrar.registrar.registerCalls, QList<quint32>{83});

  window.create();
  QTRY_COMPARE_WITH_TIMEOUT(registrar.registrar.registerCalls,
                            (QList<quint32>{83, 83}), 5'000);

  Shell::GlobalMenu::DbusMenu::DbusMenuClient client(
      consumerBus, providerBus.baseService(),
      QDBusObjectPath(QStringLiteral("/org/qindaqt/AppShell/Menu")),
      QUuid::createUuid(), 1'000);
  QVERIFY(client.start());
  QTRY_VERIFY_WITH_TIMEOUT(client.snapshot().complete, 5'000);
  QCOMPARE(client.snapshot().tree.items.constFirst().text,
           QStringLiteral("File"));

  composition.reset();
  client.stop();
  QDBusConnection::disconnectFromBus(
      QStringLiteral("menu-export-surface-composed-consumer"));
  QDBusConnection::disconnectFromBus(
      QStringLiteral("menu-export-surface-composed-provider"));
}

QTEST_MAIN(ApplicationMenuExportSurfaceTest)

#include "tst_application_menu_export_surface.moc"
