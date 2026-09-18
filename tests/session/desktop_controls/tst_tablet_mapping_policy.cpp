// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/session/desktop_controls/tablet_mapping_policy.h>
#include <qindaqt/session/desktop_controls/tablet_route_launcher.h>

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Session::DesktopControls;
using namespace QindaQt::Services::TabletDevices;

namespace {
constexpr auto kWacomIdentity = "1386:934:Wacom One Pen Display 13";
}

namespace {

// In-memory tablet authority: the same contract as the KWin port, with every
// write recorded. No bus, no device, no compositor.
class FakeTabletPort final : public TabletDevicePort {
public:
    QList<TabletDeviceSnapshot> scripted;
    QString listError;
    mutable QList<std::tuple<QString, QString, QVariant>> writes;
    mutable QString refuse; // property name the authority rejects

    [[nodiscard]] QList<TabletDeviceSnapshot>
    devices(QString *error) const override {
        if (error != nullptr) {
            *error = listError;
        }
        return listError.isEmpty() ? scripted : QList<TabletDeviceSnapshot>{};
    }

    [[nodiscard]] bool device(const QString &deviceId,
                              TabletDeviceSnapshot *snapshot,
                              QString *error) const override {
        Q_UNUSED(error)
        for (const TabletDeviceSnapshot &candidate : scripted) {
            if (candidate.deviceId == deviceId) {
                *snapshot = candidate;
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool writeProperty(const QString &deviceId,
                                     const QString &property,
                                     const QVariant &value,
                                     QString *error) const override {
        if (property == refuse) {
            if (error != nullptr) {
                *error = QStringLiteral("authority refused %1").arg(property);
            }
            return false;
        }
        writes.append({deviceId, property, value});
        // The authority's own state moves, so the next reconcile sees it.
        for (TabletDeviceSnapshot &candidate :
             const_cast<QList<TabletDeviceSnapshot> &>(scripted)) {
            if (candidate.deviceId == deviceId) {
                candidate.properties.insert(property, value);
            }
        }
        return true;
    }
};

class FakeWatcher final : public TabletDeviceWatcher {
public:
    bool started = false;
    [[nodiscard]] bool start(QString *error) override {
        Q_UNUSED(error)
        started = true;
        return true;
    }
    void plug(const QString &id) { Q_EMIT deviceAdded(id); }
    void unplug(const QString &id) { Q_EMIT deviceRemoved(id); }
};

class FakeOutputs final : public TabletOutputInventory {
public:
    QList<TabletOutputCandidate> scripted;
    [[nodiscard]] QList<TabletOutputCandidate> outputs() const override {
        return scripted;
    }
    void publish(QList<TabletOutputCandidate> next) {
        scripted = std::move(next);
        Q_EMIT outputsChanged();
    }
};

class FakeStore final : public TabletMappingStore {
public:
    TabletMappingLedger held;
    bool loaded = true;
    bool saveFails = false;
    int saves = 0;

    [[nodiscard]] bool isLoaded() const override { return loaded; }
    [[nodiscard]] TabletMappingLedger ledger() const override { return held; }
    bool save(const TabletMappingLedger &ledger) override {
        ++saves;
        if (saveFails) {
            return false;
        }
        held = ledger;
        return true;
    }
    void load(TabletMappingLedger ledger) {
        held = std::move(ledger);
        loaded = true;
        Q_EMIT ledgerChanged();
    }
};

TabletDeviceSnapshot pen(const QString &id = QStringLiteral("event19")) {
    TabletDeviceSnapshot device;
    device.deviceId = id;
    device.name = QStringLiteral("Wacom One Pen Display 13 Pen");
    // A pointer-address hash, as KWin actually supplies: nothing may key on it.
    device.deviceGroupId = QStringLiteral("5ggmGJ0A0G+Au+fGRi4fc0/veWc=");
    device.vendorId = 1386;
    device.productId = 934;
    device.tabletTool = true;
    device.properties.insert(QStringLiteral("outputName"), QString());
    device.properties.insert(QStringLiteral("mapToWorkspace"), false);
    return device;
}

TabletOutputCandidate penDisplay() {
    return TabletOutputCandidate{QStringLiteral("HDMI-A-1"),
                                 QStringLiteral("Wacom Technology Corp."),
                                 QStringLiteral("Wacom One 13"),
                                 QStringLiteral("Wacom One 13"), false, true};
}

TabletOutputCandidate laptopPanel() {
    return TabletOutputCandidate{QStringLiteral("eDP-1"),
                                 QStringLiteral("Lenovo Group Limited"),
                                 QStringLiteral("eDP-1-0x9052"),
                                 QStringLiteral("eDP-1-0x9052"), true, true};
}

} // namespace

class TabletMappingPolicyTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void mapsAPenDisplayToItsOwnScreenOnceAndAnnouncesIt();
    void aKnownTabletIsSilentOnReplug();
    void theOutputArrivingAfterTheTabletStillMapsAndSaysSo();
    void aRecordedUserChoiceIsNeverOverridden();
    void anOpaqueTabletKeepsTheCompositorDefault();
    void anUnloadedLedgerStopsThePolicyFromActing();
    void aRefusedWriteReportsAndLeavesTheLedgerAlone();
    void theUsersChoiceIsRecordedAndApplied();
    void theDeepLinkArgumentsNameTheDestinationAndDevice();
    void anUnknownTabletKeepsAMappingItAlreadyHas();
    void theLedgerSurvivesAReplugBecauseTheKeyDoes();
};

void TabletMappingPolicyTest::mapsAPenDisplayToItsOwnScreenOnceAndAnnouncesIt() {
    FakeTabletPort port;
    port.scripted.append(pen());
    FakeWatcher watcher;
    FakeOutputs outputs;
    outputs.scripted = {laptopPanel(), penDisplay()};
    FakeStore store;
    TabletMappingPolicy policy(port, watcher, outputs, store);
    QSignalSpy announced(&policy, &TabletMappingPolicy::tabletAnnounced);

    QVERIFY(policy.start());
    QVERIFY(watcher.started);

    QCOMPARE(port.writes.size(), 1);
    QCOMPARE(std::get<0>(port.writes.at(0)), QStringLiteral("event19"));
    QCOMPARE(std::get<1>(port.writes.at(0)), QStringLiteral("outputName"));
    QCOMPARE(std::get<2>(port.writes.at(0)).toString(),
             QStringLiteral("HDMI-A-1"));

    QCOMPARE(announced.size(), 1);
    QCOMPARE(announced.at(0).at(0).toString(), QLatin1String(kWacomIdentity));
    QCOMPARE(announced.at(0).at(2).toString(), QStringLiteral("HDMI-A-1"));

    const TabletMappingRecord record =
        store.held.record(QLatin1String(kWacomIdentity));
    QCOMPARE(record.choice, TabletMapChoice::NamedOutput);
    QCOMPARE(record.outputName, QStringLiteral("HDMI-A-1"));
    QVERIFY(record.announced);
    // An automatic decision must never masquerade as the user's.
    QVERIFY(!record.userChosen);
}

void TabletMappingPolicyTest::aKnownTabletIsSilentOnReplug() {
    FakeTabletPort port;
    port.scripted.append(pen());
    FakeWatcher watcher;
    FakeOutputs outputs;
    outputs.scripted = {penDisplay()};
    FakeStore store;
    TabletMappingPolicy policy(port, watcher, outputs, store);
    QVERIFY(policy.start());
    QSignalSpy announced(&policy, &TabletMappingPolicy::tabletAnnounced);

    // Unplug and plug back in: KWin persisted the mapping and so did the
    // ledger, so nothing is written and nothing is said.
    watcher.unplug(QStringLiteral("event19"));
    watcher.plug(QStringLiteral("event19"));
    QCOMPARE(announced.size(), 0);
    QCOMPARE(port.writes.size(), 1); // still only the original mapping write
}

void TabletMappingPolicyTest::theOutputArrivingAfterTheTabletStillMapsAndSaysSo() {
    // USB before HDMI: the pen is there first and its screen is not.
    FakeTabletPort port;
    port.scripted.append(pen());
    FakeWatcher watcher;
    FakeOutputs outputs;
    outputs.scripted = {laptopPanel()};
    FakeStore store;
    TabletMappingPolicy policy(port, watcher, outputs, store);
    QSignalSpy announced(&policy, &TabletMappingPolicy::tabletAnnounced);
    QVERIFY(policy.start());

    // First pass: nothing to map to, and the announcement says exactly that.
    QCOMPARE(port.writes.size(), 0);
    QCOMPARE(announced.size(), 1);
    QVERIFY(announced.at(0).at(2).toString().isEmpty());

    outputs.publish({laptopPanel(), penDisplay()});

    QCOMPARE(port.writes.size(), 1);
    QCOMPARE(std::get<2>(port.writes.at(0)).toString(),
             QStringLiteral("HDMI-A-1"));
    // The mapping changed, so the user is told again — this is the one case
    // where a known tablet speaks twice.
    QCOMPARE(announced.size(), 2);
    QCOMPARE(announced.at(1).at(2).toString(), QStringLiteral("HDMI-A-1"));
}

void TabletMappingPolicyTest::aRecordedUserChoiceIsNeverOverridden() {
    FakeTabletPort port;
    port.scripted.append(pen());
    FakeWatcher watcher;
    FakeOutputs outputs;
    outputs.scripted = {laptopPanel(), penDisplay()};
    FakeStore store;
    TabletMappingRecord chosen;
    chosen.choice = TabletMapChoice::FollowActiveScreen;
    chosen.userChosen = true;
    chosen.announced = true;
    store.held.setRecord(QLatin1String(kWacomIdentity), chosen);

    TabletMappingPolicy policy(port, watcher, outputs, store);
    QSignalSpy announced(&policy, &TabletMappingPolicy::tabletAnnounced);
    QVERIFY(policy.start());

    // The matcher would say HDMI-A-1; the user said "the active screen", and
    // the user wins forever.
    QCOMPARE(port.writes.size(), 0);
    QCOMPARE(announced.size(), 0);
    QCOMPARE(store.held.record(QLatin1String(kWacomIdentity)).choice,
             TabletMapChoice::FollowActiveScreen);
    QVERIFY(store.held.record(QLatin1String(kWacomIdentity)).userChosen);
}

void TabletMappingPolicyTest::anOpaqueTabletKeepsTheCompositorDefault() {
    FakeTabletPort port;
    TabletDeviceSnapshot opaque = pen(QStringLiteral("event21"));
    opaque.name = QStringLiteral("Some Graphics Tablet Pen");
    opaque.deviceGroupId = QStringLiteral("1386:4242:Some Graphics Tablet");
    port.scripted.append(opaque);
    FakeWatcher watcher;
    FakeOutputs outputs;
    outputs.scripted = {laptopPanel(),
                        TabletOutputCandidate{QStringLiteral("HDMI-A-1"),
                                              QStringLiteral("Dell Inc."),
                                              QStringLiteral("U2720Q"),
                                              QStringLiteral("U2720Q"), false,
                                              true}};
    FakeStore store;
    TabletMappingPolicy policy(port, watcher, outputs, store);
    QVERIFY(policy.start());

    // Nothing matched, so nothing is written: KWin's default (the active
    // output) is the honest answer for a tablet with no screen of its own.
    QCOMPARE(port.writes.size(), 0);
    QCOMPARE(store.held.record(QStringLiteral("1386:4242:Some Graphics Tablet")).choice,
             TabletMapChoice::FollowActiveScreen);
}

void TabletMappingPolicyTest::anUnloadedLedgerStopsThePolicyFromActing() {
    FakeTabletPort port;
    port.scripted.append(pen());
    FakeWatcher watcher;
    FakeOutputs outputs;
    outputs.scripted = {penDisplay()};
    FakeStore store;
    store.loaded = false;
    TabletMappingPolicy policy(port, watcher, outputs, store);
    QSignalSpy announced(&policy, &TabletMappingPolicy::tabletAnnounced);
    QVERIFY(policy.start());

    // Acting before the ledger is readable could override a user choice this
    // process has not seen yet, so it waits.
    QCOMPARE(port.writes.size(), 0);
    QCOMPARE(announced.size(), 0);

    store.load({});
    QCOMPARE(port.writes.size(), 1);
    QCOMPARE(announced.size(), 1);
}

void TabletMappingPolicyTest::aRefusedWriteReportsAndLeavesTheLedgerAlone() {
    FakeTabletPort port;
    port.scripted.append(pen());
    port.refuse = QStringLiteral("outputName");
    FakeWatcher watcher;
    FakeOutputs outputs;
    outputs.scripted = {penDisplay()};
    FakeStore store;
    TabletMappingPolicy policy(port, watcher, outputs, store);
    QSignalSpy failed(&policy, &TabletMappingPolicy::mappingFailed);
    QSignalSpy announced(&policy, &TabletMappingPolicy::tabletAnnounced);
    QVERIFY(policy.start());

    QCOMPARE(failed.size(), 1);
    QVERIFY(!failed.at(0).at(0).toString().isEmpty());
    // Nothing was mapped, so nothing is claimed as mapped and nothing is
    // announced as done.
    QCOMPARE(announced.size(), 0);
    QVERIFY(!store.held.contains(QLatin1String(kWacomIdentity)));
}

void TabletMappingPolicyTest::theUsersChoiceIsRecordedAndApplied() {
    FakeTabletPort port;
    port.scripted.append(pen());
    FakeWatcher watcher;
    FakeOutputs outputs;
    outputs.scripted = {penDisplay()};
    FakeStore store;
    TabletMappingPolicy policy(port, watcher, outputs, store);
    QVERIFY(policy.start());
    port.writes.clear();

    QString error;
    QVERIFY2(policy.applyUserChoice(QLatin1String(kWacomIdentity),
                                    TabletMapChoice::FollowActiveScreen, {},
                                    &error),
             qPrintable(error));
    QCOMPARE(port.writes.size(), 1);
    QCOMPARE(std::get<1>(port.writes.at(0)), QStringLiteral("outputName"));
    QVERIFY(std::get<2>(port.writes.at(0)).toString().isEmpty());
    const TabletMappingRecord record =
        store.held.record(QLatin1String(kWacomIdentity));
    QVERIFY(record.userChosen);
    QCOMPARE(record.choice, TabletMapChoice::FollowActiveScreen);
    QVERIFY(record.outputName.isEmpty());

    // And a later reconcile leaves it exactly there.
    port.writes.clear();
    policy.reconcile();
    QCOMPARE(port.writes.size(), 0);
}

void TabletMappingPolicyTest::theDeepLinkArgumentsNameTheDestinationAndDevice() {
    QStringList seenArguments;
    QString seenProgram;
    TabletRouteLauncher launcher(
        [&](const QString &program, const QStringList &arguments) {
            seenProgram = program;
            seenArguments = arguments;
            return true;
        });
    launcher.openTabletSettings(QLatin1String(kWacomIdentity));
    QCOMPARE(seenProgram, QStringLiteral("qindaqt-settings"));
    QCOMPARE(seenArguments,
             (QStringList{QStringLiteral("--page"), QStringLiteral("input"),
                          QStringLiteral("--destination"),
                          QStringLiteral("tablet"), QStringLiteral("--select"),
                          QLatin1String(kWacomIdentity)}));

    // No device: the route still opens, without a selection argument that
    // would name nothing.
    launcher.openTabletSettings(QString());
    QCOMPARE(seenArguments,
             (QStringList{QStringLiteral("--page"), QStringLiteral("input"),
                          QStringLiteral("--destination"),
                          QStringLiteral("tablet")}));

    QSignalSpy failed(&launcher, &TabletRouteLauncher::launchFailed);
    TabletRouteLauncher unavailable(
        [](const QString &, const QStringList &) { return false; });
    QSignalSpy unavailableFailed(&unavailable,
                                 &TabletRouteLauncher::launchFailed);
    unavailable.openTabletSettings(QLatin1String(kWacomIdentity));
    QCOMPARE(unavailableFailed.size(), 1);
    QCOMPARE(failed.size(), 0);
}

void TabletMappingPolicyTest::anUnknownTabletKeepsAMappingItAlreadyHas() {
    // AGENT-GUARD: a tablet this desktop has never recorded may still be
    // mapped correctly — by KWin's own persisted OutputUuid, or by the user
    // in another tool. Forcing it to the active screen would un-map a
    // working pen.
    FakeTabletPort port;
    TabletDeviceSnapshot mapped = pen();
    mapped.name = QStringLiteral("Unknown Tablet Pen");
    mapped.productId = 999;
    mapped.properties.insert(QStringLiteral("outputName"),
                             QStringLiteral("DP-3"));
    port.scripted.append(mapped);
    FakeWatcher watcher;
    FakeOutputs outputs;
    outputs.scripted = {laptopPanel()};
    FakeStore store;
    TabletMappingPolicy policy(port, watcher, outputs, store);
    QVERIFY(policy.start());

    // Nothing was written: the existing mapping is adopted, not replaced.
    QCOMPARE(port.writes.size(), 0);
    const TabletMappingRecord record =
        store.held.record(QStringLiteral("1386:999:Unknown Tablet"));
    QCOMPARE(record.choice, TabletMapChoice::NamedOutput);
    QCOMPARE(record.outputName, QStringLiteral("DP-3"));
    QVERIFY(!record.userChosen);

    // A workspace mapping is adopted the same way.
    FakeTabletPort spanning;
    TabletDeviceSnapshot everywhere = mapped;
    everywhere.properties.insert(QStringLiteral("outputName"), QString());
    everywhere.properties.insert(QStringLiteral("mapToWorkspace"), true);
    spanning.scripted.append(everywhere);
    FakeWatcher watcher2;
    FakeOutputs outputs2;
    outputs2.scripted = {laptopPanel()};
    FakeStore store2;
    TabletMappingPolicy policy2(spanning, watcher2, outputs2, store2);
    QVERIFY(policy2.start());
    QCOMPARE(spanning.writes.size(), 0);
    QCOMPARE(store2.held.record(QStringLiteral("1386:999:Unknown Tablet")).choice,
             TabletMapChoice::EntireWorkspace);
}

void TabletMappingPolicyTest::theLedgerSurvivesAReplugBecauseTheKeyDoes() {
    // The whole point of the identity change: KWin hands out a new
    // deviceGroupId on every re-plug, and the recorded choice must still be
    // found. A user chose "the active screen"; after a re-plug the matcher
    // must not override it.
    FakeTabletPort port;
    port.scripted.append(pen());
    FakeWatcher watcher;
    FakeOutputs outputs;
    outputs.scripted = {laptopPanel(), penDisplay()};
    FakeStore store;
    TabletMappingRecord chosen;
    chosen.choice = TabletMapChoice::FollowActiveScreen;
    chosen.userChosen = true;
    chosen.announced = true;
    store.held.setRecord(QLatin1String(kWacomIdentity), chosen);

    TabletMappingPolicy policy(port, watcher, outputs, store);
    QSignalSpy announced(&policy, &TabletMappingPolicy::tabletAnnounced);
    QVERIFY(policy.start());
    QCOMPARE(port.writes.size(), 0);
    QCOMPARE(announced.size(), 0);

    // Re-plug: a new event node AND a new device group id, as KWin really
    // supplies. Under the old key this looked like a brand-new tablet.
    TabletDeviceSnapshot replugged = pen(QStringLiteral("event27"));
    replugged.deviceGroupId = QStringLiteral("Zm9vYmFyYmF6cXV4MTIzNDU2Nzg5MA==");
    port.scripted = {replugged};
    watcher.plug(QStringLiteral("event27"));

    QCOMPARE(port.writes.size(), 0);
    QCOMPARE(announced.size(), 0);
    QVERIFY(store.held.record(QLatin1String(kWacomIdentity)).userChosen);
}

QTEST_MAIN(TabletMappingPolicyTest)
#include "tst_tablet_mapping_policy.moc"
