// SPDX-License-Identifier: GPL-3.0-or-later

// Production-composition test for the tray hosting lane: the real
// StatusNotifierAppletComposition (S1 watcher service + S2 monitor adapter +
// controller) over an ephemeral private session bus with the scripted fake
// StatusNotifierItem. Never touches the host bus.

#include "statusnotifierappletcomposition.h"

#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/shell/status_notifier/applet/status_notifier_applet_controller.h"

#include "status_notifier_fake_item_test_support.h"
#include "status_notifier_private_bus_test_support.h"

#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::StatusNotifier;
using namespace QindaQt::StatusNotifier::TestSupport;

namespace {

void registerItemOnWatcher(QDBusConnection &itemConnection, const QString &path)
{
    auto message = QDBusMessage::createMethodCall(
        QString::fromLatin1(kWatcherServiceName),
        QString::fromLatin1(kWatcherObjectPath),
        QString::fromLatin1(kWatcherInterfaceName),
        QStringLiteral("RegisterStatusNotifierItem"));
    message << QVariant(path);
    // Async + event-loop pumping: the composition's watcher lives in this
    // thread and its slots cannot dispatch while the caller blocks.
    QDBusPendingCall pending = itemConnection.asyncCall(message);
    QTRY_VERIFY_WITH_TIMEOUT(pending.isFinished(), 5'000);
    QCOMPARE(pending.reply().type(), QDBusMessage::ReplyMessage);
}

bool loadCatalogAndPolicy(Applets::ManifestCatalog *catalog,
                          AppletHost::CapabilityPolicy *policy,
                          const QString &policyPath)
{
    QString error;
    if (!catalog->loadDirectory(QStringLiteral(QINDAQT_SOURCE_DIR "/data/applets"),
                                &error)) {
        qWarning().noquote() << error;
        return false;
    }
    const auto loaded = AppletHost::CapabilityPolicyLoader::fromFile(policyPath);
    if (!loaded.ok) {
        qWarning().noquote() << loaded.error;
        return false;
    }
    *policy = loaded.policy;
    return true;
}

} // namespace

class StatusNotifierAppletCompositionPrivateBusTests final : public QObject
{
    Q_OBJECT

private slots:
    void composesPopulationDispatchOwnerLossAndAcknowledgement();
    void withholdsObservationWhenReadDenied();
};

void StatusNotifierAppletCompositionPrivateBusTests::
    composesPopulationDispatchOwnerLossAndAcknowledgement()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto compositionConnection =
        connectToPrivateBus(bus.address(), QStringLiteral("composition-a"));
    auto itemConnection =
        connectToPrivateBus(bus.address(), QStringLiteral("composition-item-a"));

    Applets::ManifestCatalog catalog;
    AppletHost::CapabilityPolicy policy;
    QVERIFY(loadCatalogAndPolicy(
        &catalog, &policy,
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/applet-policy/default.json")));

    {
        Shell::StatusNotifierAppletComposition composition(
            catalog, policy, compositionConnection, {});
        auto *controller = composition.access();
        QVERIFY(controller != nullptr);
        // Nothing registered yet: the composition's own watcher is live and
        // the observed population is empty.
        QTRY_COMPARE_WITH_TIMEOUT(controller->phaseText(), QStringLiteral("empty"),
                                  5'000);

        auto item = std::make_unique<FakeStatusNotifierItem>();
        item->title = QStringLiteral("Composition item");
        QVERIFY(registerFakeItem(itemConnection,
                                 QStringLiteral("/StatusNotifierItem"),
                                 item.get()));
        registerItemOnWatcher(itemConnection,
                              QStringLiteral("/StatusNotifierItem"));

        QTRY_COMPARE_WITH_TIMEOUT(controller->phaseText(), QStringLiteral("ready"),
                                  5'000);
        QCOMPARE(controller->itemCount(), 1);
        const QVariantList rows = controller->itemRows();
        QCOMPARE(rows.size(), 1);
        const auto row =
            rows.constFirst().value<StatusNotifierApplet::StatusNotifierItemRow>();
        const QString uniqueName = row.uniqueName;
        QCOMPARE(uniqueName, itemConnection.baseService());
        const QString objectPath = row.objectPath;
        const quint64 generation = row.generation;
        QVERIFY(generation != 0);

        // Exactly one wire Activate reaches the exact-owner fake item for one
        // admitted gesture.
        QVERIFY(controller->activateItem(uniqueName, objectPath, generation));
        QTRY_COMPARE_WITH_TIMEOUT(item->recordedIntents.size(), 1, 5'000);
        QCOMPARE(item->recordedIntents.constFirst().member,
                 QStringLiteral("Activate"));

        // A malformed live replacement degrades the composed registry while
        // retaining the last-known-good row; the acknowledgement transition
        // routed through the composition recomputes Ready from live state.
        item->title = QStringLiteral("Badtitle");
        emit item->NewTitle();
        QTRY_COMPARE_WITH_TIMEOUT(controller->phaseText(),
                                  QStringLiteral("degraded"), 5'000);
        QCOMPARE(controller->phaseReasonText(),
                 QStringLiteral("malformed-item-replacement"));
        QCOMPARE(controller->itemRows().size(), 1);
        controller->acknowledgeDegraded();
        QTRY_COMPARE_WITH_TIMEOUT(controller->phaseText(), QStringLiteral("ready"),
                                  5'000);
        QCOMPARE(controller->phaseReasonText(), QString());

        // Owner loss clears the stale truth: the live watcher observes the
        // departure and the applet returns to an honest empty state. Drop the
        // last connection reference before disconnectFromBus: while a
        // QDBusConnection copy is alive the socket stays open and the daemon
        // never emits the owner-loss signal the monitor must observe.
        item.reset();
        itemConnection = QDBusConnection(QString());
        QDBusConnection::disconnectFromBus(
            QStringLiteral("composition-item-a"));
        QTRY_COMPARE_WITH_TIMEOUT(controller->phaseText(), QStringLiteral("empty"),
                                  5'000);
        QCOMPARE(controller->itemCount(), 0);
    }

    QDBusConnection::disconnectFromBus(QStringLiteral("composition-a"));
}

void StatusNotifierAppletCompositionPrivateBusTests::
    withholdsObservationWhenReadDenied()
{
    // Negative control: an explicit status-items.read denial must withhold
    // all observation even though the composition could start its watcher and
    // a live item is registerable. This row bites if the composition starts
    // the adapter unconditionally or grants read by default.
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }

    const QString scratch = QStringLiteral(QINDAQT_TEST_SCRATCH);
    QVERIFY(QDir().mkpath(scratch));
    const QString policyPath = scratch + QStringLiteral("/deny-read-policy.json");
    QFile policyFile(policyPath);
    QVERIFY(policyFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    policyFile.write(R"({
  "schemaVersion": 1,
  "defaults": { "auditedBuiltin": "grant", "thirdParty": "deny" },
  "rules": [
    {
      "trust": "audited-builtin",
      "packageId": "status-notifier",
      "capability": "status-items.read",
      "decision": "deny",
      "reason": "Negative control: read observation is withheld."
    }
  ]
}
)");
    policyFile.close();

    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto compositionConnection =
        connectToPrivateBus(bus.address(), QStringLiteral("composition-denied"));

    Applets::ManifestCatalog catalog;
    AppletHost::CapabilityPolicy policy;
    QVERIFY(loadCatalogAndPolicy(&catalog, &policy, policyPath));

    {
        Shell::StatusNotifierAppletComposition composition(
            catalog, policy, compositionConnection, {});
        auto *controller = composition.access();
        QVERIFY(controller != nullptr);
        QCOMPARE(controller->readGranted(), false);
        QCOMPARE(controller->activateGranted(), true);
        QCOMPARE(controller->phaseText(), QStringLiteral("unavailable"));
        QCOMPARE(controller->phaseReasonText(),
                 QStringLiteral("status-items-read-not-granted"));
        QCOMPARE(controller->watcherLive(), false);
        QCOMPARE(controller->itemCount(), 0);
        // Acknowledgement fails closed to a no-op under read denial.
        controller->acknowledgeDegraded();
        QCOMPARE(controller->phaseText(), QStringLiteral("unavailable"));
    }

    QDBusConnection::disconnectFromBus(QStringLiteral("composition-denied"));
}

QTEST_GUILESS_MAIN(StatusNotifierAppletCompositionPrivateBusTests)
#include "tst_status_notifier_applet_composition_private_bus.moc"
