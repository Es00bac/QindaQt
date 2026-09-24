// SPDX-License-Identifier: GPL-3.0-or-later

#include "network_applet_test_support.h"

#include <qindaqt/shell/network_applet/network_request_state.h>

#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Shell::NetworkApplet;
using namespace QindaQt::Shell::NetworkApplet::TestSupport;
using Network::OperationKind;
using Network::OperationStatus;

namespace
{

RequestTarget target(const RequestAction action, const QString &id, const QString &label)
{
    RequestTarget value;
    value.action = action;
    value.id = id;
    value.label = label;
    return value;
}

Network::Model::ModelState stateFor(const Network::Snapshot &snapshot)
{
    Network::Model::NetworkModel model([] { return qint64{5'000}; });
    const auto applied = model.applySnapshot(snapshot);
    Q_ASSERT_X(applied.accepted, "stateFor", qPrintable(applied.reasonCode));
    return model.projection(false);
}

RequestState pending(const RequestTarget &requestTarget)
{
    return beginNetworkRequest(requestTarget, kOwner, 20, 1);
}

} // namespace

class NetworkRequestStateTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void knownConnectSucceedsOnlyFromNewerTruth();
    void visibleConnectIsRecognizedBySsidAndSecurity();
    void disconnectAndRadioReadback();
    void radioContradictionIsAConflict();
    void scanSucceedsOnAcceptedReply();
    void foreignOrStaleRepliesAreUncertain();
    void refusalsAndFailuresExplainThemselves();
    void ownerLossAndDeadlineRetireWithoutReplay();
    void settledRequestsIgnoreLateInput();
};

void NetworkRequestStateTests::knownConnectSucceedsOnlyFromNewerTruth()
{
    RequestState request = pending(target(RequestAction::ConnectKnown, cafeId(),
                                          QStringLiteral("Cafe")));
    QCOMPARE(request.phase, RequestPhase::Pending);
    QVERIFY(request.active());
    QCOMPARE(request.feedback, QStringLiteral("Connecting to Cafe…"));

    request = applyNetworkResult(request, operationResult(OperationKind::ConnectKnownNetwork,
                                                          OperationStatus::Succeeded));
    QCOMPARE(request.phase, RequestPhase::Confirming);
    QVERIFY(request.active());

    Network::Snapshot connected = appletSnapshot(1);
    connected.activeConnections = {{QStringLiteral("wlan0"), cafeId()}};
    // Same revision: the reply alone never proves the new state.
    QCOMPARE(observeNetworkState(request, stateFor(connected), true).phase,
             RequestPhase::Confirming);
    Network::Snapshot unchanged = appletSnapshot(2);
    QCOMPARE(observeNetworkState(request, stateFor(unchanged), true).phase,
             RequestPhase::Confirming); // still activating: keep waiting
    connected.revision = 3;
    QCOMPARE(observeNetworkState(request, stateFor(connected), false).phase,
             RequestPhase::Confirming); // not current: no verdict
    const RequestState done = observeNetworkState(request, stateFor(connected), true);
    QCOMPARE(done.phase, RequestPhase::Succeeded);
    QCOMPARE(done.feedback, QStringLiteral("Connected to Cafe."));
    QVERIFY(!done.active());
}

void NetworkRequestStateTests::visibleConnectIsRecognizedBySsidAndSecurity()
{
    RequestTarget guest = target(RequestAction::ConnectVisible, guestPoint(),
                                 QStringLiteral("Guest"));
    guest.ssid = QStringLiteral("Guest");
    guest.security = Network::SecuritySuite::Open;
    RequestState request = pending(guest);
    QVERIFY(request.feedback.contains(QStringLiteral("password prompt")));
    request = applyNetworkResult(request, operationResult(OperationKind::ConnectVisibleNetwork,
                                                          OperationStatus::Succeeded));
    QCOMPARE(request.phase, RequestPhase::Confirming);

    Network::Snapshot created = appletSnapshot(2);
    created.knownNetworks.append({guestKnownId(), QStringLiteral("Guest"), false,
                                  Network::SecuritySuite::Wpa2Personal, true});
    created.activeConnections = {{QStringLiteral("wlan0"), guestKnownId()}};
    // A same-name profile of a different security is not this network.
    QCOMPARE(observeNetworkState(request, stateFor(created), true).phase,
             RequestPhase::Confirming);
    created.knownNetworks.last().security = Network::SecuritySuite::Open;
    QCOMPARE(observeNetworkState(request, stateFor(created), true).phase,
             RequestPhase::Succeeded);
}

void NetworkRequestStateTests::disconnectAndRadioReadback()
{
    RequestState disconnect = applyNetworkResult(
        pending(target(RequestAction::Disconnect, QStringLiteral("wlan0"), QStringLiteral("Home"))),
        operationResult(OperationKind::DisconnectActive, OperationStatus::Succeeded));
    Network::Snapshot idle = appletSnapshot(2);
    idle.activeConnections.clear();
    const RequestState disconnected = observeNetworkState(disconnect, stateFor(idle), true);
    QCOMPARE(disconnected.phase, RequestPhase::Succeeded);
    QCOMPARE(disconnected.feedback, QStringLiteral("Disconnected Home."));

    RequestTarget radio = target(RequestAction::SetRadio, QStringLiteral("wifi"),
                                 QStringLiteral("Wi-Fi"));
    radio.enable = false;
    RequestState off = pending(radio);
    QCOMPARE(off.feedback, QStringLiteral("Turning Wi-Fi off…"));
    off = applyNetworkResult(off, operationResult(OperationKind::SetRadio,
                                                  OperationStatus::Succeeded));
    Network::Snapshot radioOff = idle;
    radioOff.radios[0].softwareEnabled = false;
    const RequestState confirmed = observeNetworkState(off, stateFor(radioOff), true);
    QCOMPARE(confirmed.phase, RequestPhase::Succeeded);
    QCOMPARE(confirmed.feedback, QStringLiteral("Wi-Fi is off."));
}

void NetworkRequestStateTests::radioContradictionIsAConflict()
{
    RequestTarget radio = target(RequestAction::SetRadio, QStringLiteral("wifi"),
                                 QStringLiteral("Wi-Fi"));
    radio.enable = false;
    const RequestState confirming = applyNetworkResult(
        pending(radio), operationResult(OperationKind::SetRadio, OperationStatus::Succeeded));
    const RequestState conflict =
        observeNetworkState(confirming, stateFor(appletSnapshot(2)), true);
    QCOMPARE(conflict.phase, RequestPhase::Failed);
    QVERIFY(conflict.feedback.contains(QStringLiteral("still reports it on")));
}

void NetworkRequestStateTests::scanSucceedsOnAcceptedReply()
{
    const RequestState scan = applyNetworkResult(
        pending(target(RequestAction::Scan, {}, QStringLiteral("networks"))),
        operationResult(OperationKind::RequestScan, OperationStatus::Succeeded));
    QCOMPARE(scan.phase, RequestPhase::Succeeded);
    QVERIFY(scan.feedback.contains(QStringLiteral("Scan requested")));
}

void NetworkRequestStateTests::foreignOrStaleRepliesAreUncertain()
{
    const RequestState request = pending(target(RequestAction::ConnectKnown, cafeId(),
                                                QStringLiteral("Cafe")));
    QCOMPARE(applyNetworkResult(request, operationResult(OperationKind::DisconnectActive,
                                                         OperationStatus::Succeeded))
                 .phase,
             RequestPhase::Uncertain);
    QCOMPARE(applyNetworkResult(request, operationResult(OperationKind::ConnectKnownNetwork,
                                                         OperationStatus::Succeeded, 20, 7))
                 .phase,
             RequestPhase::Uncertain);
    QCOMPARE(applyNetworkResult(request, operationResult(OperationKind::ConnectKnownNetwork,
                                                         OperationStatus::Succeeded, 21, 1))
                 .phase,
             RequestPhase::Uncertain);
    Network::OperationResult invalid = operationResult(OperationKind::ConnectKnownNetwork,
                                                       OperationStatus::Succeeded);
    invalid.wireValid = false;
    const RequestState unreadable = applyNetworkResult(request, invalid);
    QCOMPARE(unreadable.phase, RequestPhase::Uncertain);
    QVERIFY(unreadable.feedback.contains(QStringLiteral("before trying again")));
    QCOMPARE(applyNetworkResult(request, operationResult(OperationKind::ConnectKnownNetwork,
                                                         OperationStatus::Uncertain))
                 .phase,
             RequestPhase::Uncertain);
}

void NetworkRequestStateTests::refusalsAndFailuresExplainThemselves()
{
    const RequestTarget cafe = target(RequestAction::ConnectKnown, cafeId(),
                                      QStringLiteral("Cafe"));
    const RequestState refused = refuseNetworkRequest(cafe, QStringLiteral("operation-in-flight"));
    QCOMPARE(refused.phase, RequestPhase::Failed);
    QCOMPARE(refused.feedback, QStringLiteral(
        "Could not connect to Cafe: another network change is in progress."));
    QVERIFY(refused.owner.isEmpty());

    const RequestState rejected = applyNetworkResult(
        pending(cafe), operationResult(OperationKind::ConnectKnownNetwork,
                                       OperationStatus::Rejected, 20, 1,
                                       QStringLiteral("credentials-required")));
    QCOMPARE(rejected.phase, RequestPhase::Failed);
    QVERIFY(rejected.feedback.contains(QStringLiteral("password is required")));
    const RequestState busy = applyNetworkResult(
        pending(cafe), operationResult(OperationKind::ConnectKnownNetwork,
                                       OperationStatus::Busy));
    QVERIFY(busy.feedback.contains(QStringLiteral("busy")));

    RequestTarget radio = target(RequestAction::SetRadio, QStringLiteral("wifi"),
                                 QStringLiteral("Wi-Fi"));
    radio.enable = true;
    QVERIFY(refuseNetworkRequest(radio, QStringLiteral("radio-hardware-disabled"))
                .feedback.contains(QStringLiteral("hardware switch")));
    QVERIFY(!networkReasonText(QStringLiteral("unrecognized-code")).isEmpty());
}

void NetworkRequestStateTests::ownerLossAndDeadlineRetireWithoutReplay()
{
    const RequestState confirming = applyNetworkResult(
        pending(target(RequestAction::ConnectKnown, cafeId(), QStringLiteral("Cafe"))),
        operationResult(OperationKind::ConnectKnownNetwork, OperationStatus::Succeeded));
    Network::Snapshot replaced = readySnapshot(QStringLiteral(":1.99"), 21, 1);
    replaced.activeConnections = {{QStringLiteral("wlan0"), cafeId()}};
    const RequestState foreign = observeNetworkState(confirming, stateFor(replaced), true);
    QCOMPARE(foreign.phase, RequestPhase::Uncertain);
    QVERIFY(foreign.feedback.contains(QStringLiteral("service changed")));

    QCOMPARE(observeNetworkState(confirming, Network::Model::ModelState{}, false).phase,
             RequestPhase::Uncertain);
    QCOMPARE(applyNetworkUncertain(confirming).phase, RequestPhase::Uncertain);
    const RequestState expired = expireNetworkRequest(confirming);
    QCOMPARE(expired.phase, RequestPhase::Uncertain);
    QVERIFY(expired.feedback.contains(QStringLiteral("not confirmed in time")));
}

void NetworkRequestStateTests::settledRequestsIgnoreLateInput()
{
    const RequestState failed = refuseNetworkRequest(
        target(RequestAction::Scan, {}, QStringLiteral("networks")),
        QStringLiteral("scan-busy"));
    QCOMPARE(applyNetworkResult(failed, operationResult(OperationKind::RequestScan,
                                                        OperationStatus::Succeeded)),
             failed);
    QCOMPARE(applyNetworkUncertain(failed), failed);
    QCOMPARE(expireNetworkRequest(failed), failed);
    QCOMPARE(observeNetworkState(failed, Network::Model::ModelState{}, false), failed);
    // A second reply to a request already confirming is not re-applied.
    const RequestState confirming = applyNetworkResult(
        pending(target(RequestAction::Disconnect, QStringLiteral("wlan0"), QStringLiteral("Home"))),
        operationResult(OperationKind::DisconnectActive, OperationStatus::Succeeded));
    QCOMPARE(applyNetworkResult(confirming, operationResult(OperationKind::DisconnectActive,
                                                            OperationStatus::Failed)),
             confirming);
}

QTEST_GUILESS_MAIN(NetworkRequestStateTests)
#include "tst_network_request_state.moc"
