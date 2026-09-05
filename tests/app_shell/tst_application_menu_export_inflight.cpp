// SPDX-License-Identifier: GPL-3.0-or-later
// In-flight registration rows for the first-party menu export. A registrar
// can accept RegisterWindow and mutate its registry before the exporter ever
// sees the reply, so surface destruction in that window must compensate the
// attempted id exactly once — the reply that arrives later is superseded and
// must change nothing. These rows are the P1-01 negative controls for the
// surface-recreation lifecycle proven in tst_application_menu_export_surface.
#include <qindaqt/app_shell/application_coordinator.h>
#include <qindaqt/app_shell/menu_export/application_menu_export.h>
#include <qindaqt/shell/global_menu/registrar/registrar_wire.h>

#include <QDBusContext>
#include <QDBusError>
#include <QDBusMessage>
#include <QTimer>
#include <QWindow>
#include <QtTest>

#include <memory>
#include <optional>

using namespace QindaQt;

namespace {

// Records every call in arrival order. RegisterWindow can hold its success
// reply back (creating the accept-now-answer-later race) or refuse with an
// explicit D-Bus error, so a row controls exactly what the exporter knows
// when the surface is destroyed.
class RacingRegistrar final : public QObject, protected QDBusContext {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "com.canonical.AppMenu.Registrar")

public:
  QList<quint32> registerCalls;
  QList<quint32> unregisterCalls;
  QStringList callLog;
  int repliesSent = 0;
  bool rejectRegistrations = false;

public Q_SLOTS:
  Q_SCRIPTABLE void RegisterWindow(quint32 id, const QDBusObjectPath &path) {
    (void)path;
    registerCalls.append(id);
    callLog.append(QStringLiteral("register:%1").arg(id));
    if (rejectRegistrations) {
      sendErrorReply(QDBusError::AccessDenied,
                     QStringLiteral("rejected registration"));
      return;
    }
    setDelayedReply(true);
    const QDBusMessage call = message();
    const QDBusConnection bus = connection();
    QTimer::singleShot(750, this, [this, call, bus] {
      bus.send(call.createReply());
      ++repliesSent;
    });
  }

  Q_SCRIPTABLE void UnregisterWindow(quint32 id) {
    unregisterCalls.append(id);
    callLog.append(QStringLiteral("unregister:%1").arg(id));
  }
};

// Publishes 71 for the original surface and 72 for any recreated surface, so
// a row can prove compensation targeted the stale attempted id.
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
  RacingRegistrar registrar;

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

  ~RegistrarFixture() {
    connection.unregisterService(QString::fromLatin1(
        Shell::GlobalMenu::Registrar::kRegistrarServiceName));
    connection.unregisterObject(QString::fromLatin1(
        Shell::GlobalMenu::Registrar::kRegistrarObjectPath));
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

} // namespace

class ApplicationMenuExportInFlightTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void surfaceDestructionCompensatesInFlightRegistrationExactlyOnce();
  void rejectedInFlightRegistrationNeedsNoUnregisterAndDoesNotCrash();
};

void ApplicationMenuExportInFlightTest::
    surfaceDestructionCompensatesInFlightRegistrationExactlyOnce() {
  RegistrarFixture registrar(
      QStringLiteral("menu-export-inflight-registrar"));
  QVERIFY(registrar.start());
  const QString providerName =
      QStringLiteral("menu-export-inflight-provider");
  auto providerBus =
      QDBusConnection::connectToBus(QDBusConnection::SessionBus, providerName);
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
  // The registrar has recorded RegisterWindow(71) but has not answered: the
  // registration is in flight and its outcome unknown to the exporter.
  QTRY_COMPARE_WITH_TIMEOUT(registrar.registrar.registerCalls,
                            QList<quint32>{71}, 5'000);
  QCOMPARE(registrar.registrar.repliesSent, 0);
  QCOMPARE(composition.status(),
           AppShell::MenuExport::MenuExportStatus::Registering);
  QVERIFY(!composition.registeredWindowId().has_value());

  // Destroying the surface in that window must withdraw the attempted id —
  // the registrar may already hold it — exactly once, before any republish.
  window.destroy();
  QCOMPARE(composition.status(),
           AppShell::MenuExport::MenuExportStatus::WaitingForRegistrar);
  QCOMPARE(composition.failureCode(),
           QStringLiteral("window-surface-destroyed"));
  QTRY_COMPARE_WITH_TIMEOUT(registrar.registrar.unregisterCalls,
                            QList<quint32>{71}, 5'000);
  QCOMPARE(probe->withdrawCount, 1);
  QVERIFY(!composition.registeredWindowId().has_value());

  // Recreating the surface publishes 72; the registrar must have observed the
  // 71 compensation strictly before the new registration.
  window.create();
  QTRY_VERIFY_WITH_TIMEOUT(composition.published(), 5'000);
  QCOMPARE(composition.registeredWindowId(), std::optional<quint32>{72});
  QCOMPARE(probe->publishCount, 2);
  QCOMPARE(probe->withdrawCount, 1);
  QCOMPARE(registrar.registrar.callLog,
           (QStringList{QStringLiteral("register:71"),
                        QStringLiteral("unregister:71"),
                        QStringLiteral("register:72")}));

  // The delayed replies for both attempts finally arrive; the superseded 71
  // reply must change nothing: no state flip, no second compensation, no
  // crash.
  QTRY_COMPARE_WITH_TIMEOUT(registrar.registrar.repliesSent, 2, 5'000);
  QTest::qWait(200);
  QVERIFY(composition.published());
  QCOMPARE(composition.registeredWindowId(), std::optional<quint32>{72});
  QCOMPARE(registrar.registrar.unregisterCalls, QList<quint32>{71});
  QCOMPARE(registrar.registrar.registerCalls, (QList<quint32>{71, 72}));

  // Stop is a withdrawal too: the reply-confirmed 72 is compensated once.
  composition.stop();
  QTRY_COMPARE_WITH_TIMEOUT(
      registrar.registrar.callLog,
      (QStringList{QStringLiteral("register:71"),
                   QStringLiteral("unregister:71"),
                   QStringLiteral("register:72"),
                   QStringLiteral("unregister:72")}),
      5'000);
  QDBusConnection::disconnectFromBus(providerName);
}

void ApplicationMenuExportInFlightTest::
    rejectedInFlightRegistrationNeedsNoUnregisterAndDoesNotCrash() {
  RegistrarFixture registrar(
      QStringLiteral("menu-export-inflight-rejected"));
  registrar.registrar.rejectRegistrations = true;
  QVERIFY(registrar.start());
  const QString providerName =
      QStringLiteral("menu-export-inflight-rejected-provider");
  auto providerBus =
      QDBusConnection::connectToBus(QDBusConnection::SessionBus, providerName);
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
  // An answered error is authoritative: the registrar refused the attempt,
  // no id is live, and the exporter fails closed without crashing.
  QTRY_COMPARE_WITH_TIMEOUT(composition.failureCode(),
                           QStringLiteral("registrar-registration-failed"),
                           5'000);
  QCOMPARE(composition.status(),
           AppShell::MenuExport::MenuExportStatus::WaitingForRegistrar);
  QCOMPARE(registrar.registrar.registerCalls, QList<quint32>{71});
  QVERIFY(!composition.registeredWindowId().has_value());
  QVERIFY(!composition.published());

  window.destroy();
  QTest::qWait(200);
  QCOMPARE(registrar.registrar.unregisterCalls, QList<quint32>{});
  QCOMPARE(composition.failureCode(),
           QStringLiteral("window-surface-destroyed"));
  QCOMPARE(probe->withdrawCount, 1);

  // Recreation attempts a fresh registration; it is refused again and still
  // owes no compensation, through stop as well.
  window.create();
  QTRY_COMPARE_WITH_TIMEOUT(registrar.registrar.registerCalls,
                            (QList<quint32>{71, 72}), 5'000);
  QTRY_COMPARE_WITH_TIMEOUT(composition.failureCode(),
                           QStringLiteral("registrar-registration-failed"),
                           5'000);
  QTest::qWait(150);
  QCOMPARE(registrar.registrar.unregisterCalls, QList<quint32>{});
  QCOMPARE(probe->publishCount, 2);
  QVERIFY(!composition.published());

  composition.stop();
  QTest::qWait(200);
  QCOMPARE(registrar.registrar.unregisterCalls, QList<quint32>{});
  QDBusConnection::disconnectFromBus(providerName);
}

QTEST_MAIN(ApplicationMenuExportInFlightTest)

#include "tst_application_menu_export_inflight.moc"
