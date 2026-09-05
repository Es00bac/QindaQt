// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop_controls_test_support.h"
#include "desktopcontrolscomposition.h"

#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/shell/desktop_controls/command_search_controller.h"
#include "qindaqt/shell/desktop_controls/desktop_controls_access.h"
#include "qindaqt/shell/desktop_controls/quick_launch_controller.h"
#include "qindaqt/shell/desktop_controls/system_status_controller.h"

#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Shell;
using namespace QindaQt::Tests::DesktopControls;

namespace {

struct Catalog {
  Applets::ManifestCatalog manifests;
  AppletHost::CapabilityPolicy policy;

  bool load(QString *error)
  {
    if (!manifests.loadDirectory(QStringLiteral(QINDAQT_SOURCE_DIR "/data/applets"), error)) {
      return false;
    }
    const auto loaded = AppletHost::CapabilityPolicyLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/applet-policy/default.json"));
    if (!loaded.ok) {
      *error = loaded.error;
      return false;
    }
    policy = loaded.policy;
    return true;
  }
};

} // namespace

class DesktopControlsCompositionTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void stockPolicyComposesEveryFacadeOverInjectedSeams();
  void denyingPolicyWithholdsWorkspaceAndLaunchAuthority();
};

void DesktopControlsCompositionTests::stockPolicyComposesEveryFacadeOverInjectedSeams()
{
  Catalog catalog;
  QString error;
  QVERIFY2(catalog.load(&error), qPrintable(error));
  QindaQt::Tests::Workspaces::FakeWorkspaceTransport transport;
  RecordingFolderOpener opener;
  LauncherStack launcherStack;
  QVERIFY(launcherStack.scanner.start());
  Launcher::LauncherAppletController launcher(&launcherStack.scanner, nullptr,
                                              &launcherStack.executor, true);
  StubSessionActions session;

  DesktopControlsComposition composition(
      catalog.manifests, catalog.policy, transport, opener,
      DesktopControlsComposition::BorrowedFacades{&launcher, nullptr, nullptr, nullptr,
                                                 nullptr, nullptr, &session});
  auto *access = composition.access();
  QVERIFY(access != nullptr);
  QVERIFY(access->workspaces() != nullptr);
  QVERIFY(access->systemStatus() != nullptr);
  QVERIFY(access->systemMenu() != nullptr);
  QVERIFY(access->places() != nullptr);
  QVERIFY(access->quickLaunch() != nullptr);
  QVERIFY(access->activeApplication() != nullptr);
  QVERIFY(access->commandPalette() != nullptr);
  QVERIFY(access->commandHud() != nullptr);
  QVERIFY(access->overview() != nullptr);
  QCOMPARE(access->launcher(), &launcher);
  QCOMPARE(access->systemMenu()->property("sessionActions").value<QObject *>(), &session);

  // Grants from the stock policy reach each facade.
  auto *workspaces = composition.workspaces();
  QVERIFY(workspaces->readGranted());
  QVERIFY(workspaces->manageGranted());
  QCOMPARE(access->quickLaunch()->property("launchGranted").toBool(), true);
  QCOMPARE(access->commandHud()->property("sourceKinds").toStringList(),
           QStringList{QStringLiteral("menuAction")});
  QCOMPARE(access->overview()->property("sourceKinds").toStringList(),
           QStringList({QStringLiteral("window"), QStringLiteral("workspace"),
                        QStringLiteral("application")}));
  QCOMPARE(access->systemStatus()->property("laneCount").toInt(), 0); // no service facades

  // Lifetimes: start/stop reach the injected transport exactly once each.
  QCOMPARE(transport.startCalls, 0);
  QVERIFY2(composition.start(&error), qPrintable(error));
  QCOMPARE(transport.startCalls, 1);
  QVERIFY(composition.start(&error));
  QCOMPARE(transport.startCalls, 1);
  transport.announce(QStringLiteral(":1.7"));
  QCOMPARE(transport.snapshotRequests.size(), 1);
  composition.stop();
  QCOMPARE(transport.stopCalls, 1);
  composition.stop();
  QCOMPARE(transport.stopCalls, 1);
}

void DesktopControlsCompositionTests::denyingPolicyWithholdsWorkspaceAndLaunchAuthority()
{
  Catalog catalog;
  QString error;
  QVERIFY2(catalog.load(&error), qPrintable(error));
  AppletHost::CapabilityPolicy deny = catalog.policy;
  deny.auditedBuiltinDefault = AppletHost::CapabilityDisposition::Deny;
  deny.rules.clear();

  QindaQt::Tests::Workspaces::FakeWorkspaceTransport transport;
  RecordingFolderOpener opener;
  DesktopControlsComposition composition(catalog.manifests, deny, transport, opener, {});
  auto *access = composition.access();
  QVERIFY(access != nullptr);
  // Every manifest still resolves (host + registry), but with no grants the
  // workspace controller is not even created and the transport never starts.
  QCOMPARE(access->workspaces(), nullptr);
  QCOMPARE(composition.workspaces(), nullptr);
  QVERIFY(access->quickLaunch() != nullptr);
  QCOMPARE(access->quickLaunch()->property("launchGranted").toBool(), false);
  QCOMPARE(access->places()->property("launchGranted").toBool(), false);
  QCOMPARE(access->launcher(), nullptr);
  QVERIFY(composition.start(&error));
  QCOMPARE(transport.startCalls, 0);
  QCOMPARE(access->commandPalette()->property("available").toBool(), false);
}

QTEST_GUILESS_MAIN(DesktopControlsCompositionTests)
#include "tst_desktop_controls_composition.moc"
