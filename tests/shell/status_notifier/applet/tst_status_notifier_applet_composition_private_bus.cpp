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
#include "status_notifier_strict_item_test_support.h"

#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusVariant>
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

// Reads IsStatusNotifierHostRegistered the way a conformant item does, from a
// connection that is neither the composition's nor the item's.
[[nodiscard]] bool readHostRegistered(QDBusConnection &connection, bool *ok)
{
    auto message = QDBusMessage::createMethodCall(
        QString::fromLatin1(kWatcherServiceName),
        QString::fromLatin1(kWatcherObjectPath),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("Get"));
    message << QString::fromLatin1(kWatcherInterfaceName)
            << QStringLiteral("IsStatusNotifierHostRegistered");
    QDBusPendingCall pending = connection.asyncCall(message);
    while (!pending.isFinished()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    const QDBusMessage reply = pending.reply();
    *ok = reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty();
    if (!*ok) {
        return false;
    }
    return reply.arguments().constFirst().value<QDBusVariant>().variant().toBool();
}

// True when any org.kde.StatusNotifierHost-* name is owned on the bus. Used
// where no watcher exists to answer the property, so the only observable fact
// is whether this process announced itself as a host at all.
[[nodiscard]] bool anyHostNameOwned(QDBusConnection &connection, bool *ok)
{
    auto message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("ListNames"));
    QDBusPendingCall pending = connection.asyncCall(message);
    while (!pending.isFinished()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    const QDBusMessage reply = pending.reply();
    *ok = reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty();
    if (!*ok) {
        return false;
    }
    const QStringList names = reply.arguments().constFirst().toStringList();
    for (const QString &name : names) {
        if (name.startsWith(QStringLiteral("org.kde.StatusNotifierHost-"))) {
            return true;
        }
    }
    return false;
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
    void announcesAHostSoConformantItemsPresentThemselves();
    void projectsAPixmapOnlyItemWithNoExportedMenu();
    void projectsAStrictWineItemRegisteredAfterHostStart();
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
        // ADR-0166: a shell that cannot observe items must not announce a host
        // either, or every item would be told a tray exists while nothing
        // could ever draw it. No watcher runs under read denial, so the
        // observable fact is that no host name was claimed.
        auto probeConnection =
            connectToPrivateBus(bus.address(), QStringLiteral("composition-denied-probe"));
        bool ok = false;
        QCOMPARE(anyHostNameOwned(probeConnection, &ok), false);
        QVERIFY(ok);
        QDBusConnection::disconnectFromBus(QStringLiteral("composition-denied-probe"));
    }

    QDBusConnection::disconnectFromBus(QStringLiteral("composition-denied"));
}

// ADR-0166 regression. On the user's live session the shell owned
// org.kde.StatusNotifierWatcher while IsStatusNotifierHostRegistered stayed
// false, because nothing ever called RegisterStatusNotifierHost. A conformant
// item is entitled to hide its icon or fall back to the legacy XEmbed tray in
// that state, which is how a "broken" tray presents to a user. This row fails
// on any build where the composition serves a watcher without announcing a
// host.
void StatusNotifierAppletCompositionPrivateBusTests::
    announcesAHostSoConformantItemsPresentThemselves()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto compositionConnection =
        connectToPrivateBus(bus.address(), QStringLiteral("composition-host"));
    auto itemConnection =
        connectToPrivateBus(bus.address(), QStringLiteral("composition-host-item"));

    Applets::ManifestCatalog catalog;
    AppletHost::CapabilityPolicy policy;
    QVERIFY(loadCatalogAndPolicy(
        &catalog, &policy,
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/applet-policy/default.json")));

    {
        Shell::StatusNotifierAppletComposition composition(
            catalog, policy, compositionConnection, {});
        QVERIFY(composition.access() != nullptr);

        bool ok = false;
        QTRY_VERIFY_WITH_TIMEOUT(readHostRegistered(itemConnection, &ok), 5'000);
        QVERIFY(ok);
    }

    QDBusConnection::disconnectFromBus(QStringLiteral("composition-host"));
    QDBusConnection::disconnectFromBus(QStringLiteral("composition-host-item"));
}

// The exact shape of a real Wine tray item observed on the user's live session
// (Battle.net, Id "wine-0x100de-0"): an EMPTY IconName with a 16x16 ARGB
// IconPixmap, no exported dbusmenu (Menu is the placeholder "/NO_DBUSMENU"),
// no overlay and no tooltip. Such an item must reach Ready and render from its
// pixmap rather than being dropped or drawn as a placeholder.
void StatusNotifierAppletCompositionPrivateBusTests::
    projectsAPixmapOnlyItemWithNoExportedMenu()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto compositionConnection =
        connectToPrivateBus(bus.address(), QStringLiteral("composition-wine"));
    auto itemConnection =
        connectToPrivateBus(bus.address(), QStringLiteral("composition-wine-item"));

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

        auto item = std::make_unique<FakeStatusNotifierItem>();
        item->id = QStringLiteral("wine-0x100de-0");
        item->title = QStringLiteral("Battle.net");
        item->category = QStringLiteral("ApplicationStatus");
        item->status = QStringLiteral("Active");
        item->iconName = QString();
        item->overlayIconName = QString();
        item->attentionIconName = QString();
        item->itemIsMenu = false;
        item->menu = QDBusObjectPath(QStringLiteral("/NO_DBUSMENU"));
        // Opaque mid-grey 16x16 in ARGB32 byte order, as a real item sends it.
        QByteArray argb;
        argb.reserve(16 * 16 * 4);
        for (int pixel = 0; pixel < 16 * 16; ++pixel) {
            argb.append(char(0xFF));
            argb.append(char(0x80));
            argb.append(char(0x80));
            argb.append(char(0x80));
        }
        item->iconPixmap = {FakeStatusNotifierItem::pixmap(16, 16, argb)};

        QVERIFY(registerFakeItem(itemConnection,
                                 QStringLiteral("/StatusNotifierItem"),
                                 item.get()));
        registerItemOnWatcher(itemConnection, QStringLiteral("/StatusNotifierItem"));

        QTRY_COMPARE_WITH_TIMEOUT(controller->phaseText(), QStringLiteral("ready"),
                                  5'000);
        QCOMPARE(controller->itemCount(), 1);
        const auto row = controller->itemRows().constFirst()
                             .value<StatusNotifierApplet::StatusNotifierItemRow>();
        QCOMPARE(row.identity, QStringLiteral("wine-0x100de-0"));
        QCOMPARE(row.title, QStringLiteral("Battle.net"));
        // No exported menu must neither suppress the item nor be mistaken for
        // a real menu.
        QCOMPARE(row.hasMenu, false);
        QCOMPARE(controller->hasExportedMenu(row.uniqueName, row.objectPath,
                                             row.generation),
                 false);
        // The decisive assertion: the item is drawn from its wire pixmap, not
        // from the neutral placeholder an icon-name-less item would otherwise
        // fall back to.
        QCOMPARE(row.iconIsPlaceholder, false);
        QVERIFY(row.iconDataUrl.startsWith(QStringLiteral("data:image/png;base64,")));
    }

    QDBusConnection::disconnectFromBus(QStringLiteral("composition-wine"));
    QDBusConnection::disconnectFromBus(QStringLiteral("composition-wine-item"));
}

// Live regression (2026-09-25): Wine and GDBus items registered, the watcher
// broadcast them, and the tray still stayed empty, because the item client
// sent GetAll without an interface header and strict items refuse that. The
// lenient fake above is served by QtDBus and accepts header-less calls, so it
// could not see the defect; this strict item routes by interface like Wine,
// and registers only after the host is up, as a game launched mid-session does.
void StatusNotifierAppletCompositionPrivateBusTests::
    projectsAStrictWineItemRegisteredAfterHostStart()
{
    if (QStandardPaths::findExecutable(QStringLiteral("dbus-daemon")).isEmpty()) {
        QSKIP("dbus-daemon is unavailable");
    }
    PrivateSessionBus bus;
    QString error;
    QVERIFY2(bus.start(&error), qPrintable(error));
    auto compositionConnection =
        connectToPrivateBus(bus.address(), QStringLiteral("composition-strict"));
    auto itemConnection =
        connectToPrivateBus(bus.address(), QStringLiteral("composition-strict-item"));
    auto probeConnection =
        connectToPrivateBus(bus.address(), QStringLiteral("composition-strict-probe"));

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
        QTRY_COMPARE_WITH_TIMEOUT(controller->phaseText(), QStringLiteral("empty"),
                                  5'000);
        bool ok = false;
        QTRY_VERIFY_WITH_TIMEOUT(readHostRegistered(probeConnection, &ok), 5'000);
        QVERIFY(ok);

        StrictWineStatusNotifierItem item;
        QVERIFY(item.registerOn(itemConnection, QStringLiteral("/StatusNotifierItem")));

        // Control: the fake really is strict. A header-less GetAll gets
        // Wine's UnknownMethod; the named interface gets the property set.
        const auto getAll = [&](const QString &interface) {
            auto message = QDBusMessage::createMethodCall(
                itemConnection.baseService(), QStringLiteral("/StatusNotifierItem"),
                interface, QStringLiteral("GetAll"));
            message << QString::fromLatin1(kItemInterfaceName);
            QDBusPendingCall pending = probeConnection.asyncCall(message);
            while (!pending.isFinished()) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            }
            return pending.reply();
        };
        const QDBusMessage refused = getAll(QString());
        QCOMPARE(refused.type(), QDBusMessage::ErrorMessage);
        QCOMPARE(refused.errorName(),
                 QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"));
        QCOMPARE(getAll(QString::fromLatin1(kPropertiesInterfaceName)).type(),
                 QDBusMessage::ReplyMessage);
        item.emptyInterfaceRejections = 0;

        registerItemOnWatcher(itemConnection, QStringLiteral("/StatusNotifierItem"));

        QTRY_COMPARE_WITH_TIMEOUT(controller->phaseText(), QStringLiteral("ready"),
                                  5'000);
        QCOMPARE(item.emptyInterfaceRejections, 0);
        QCOMPARE(controller->itemCount(), 1);
        const auto row = controller->itemRows().constFirst()
                             .value<StatusNotifierApplet::StatusNotifierItemRow>();
        QCOMPARE(row.uniqueName, itemConnection.baseService());
        QCOMPARE(row.identity, QStringLiteral("wine-0x100fe-0"));
        QCOMPARE(row.title, QStringLiteral("Battle.net"));
        QCOMPARE(row.hasMenu, false);
        QCOMPARE(row.iconIsPlaceholder, false);
        QVERIFY(row.iconDataUrl.startsWith(QStringLiteral("data:image/png;base64,")));

        itemConnection.unregisterObject(QStringLiteral("/StatusNotifierItem"));
    }

    QDBusConnection::disconnectFromBus(QStringLiteral("composition-strict"));
    QDBusConnection::disconnectFromBus(QStringLiteral("composition-strict-item"));
    QDBusConnection::disconnectFromBus(QStringLiteral("composition-strict-probe"));
}

QTEST_GUILESS_MAIN(StatusNotifierAppletCompositionPrivateBusTests)
#include "tst_status_notifier_applet_composition_private_bus.moc"
