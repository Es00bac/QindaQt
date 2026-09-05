// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktopnotificationshellreadiness.h"
#include "desktopnotificationshellpolishfixtures.h"

#include <QJsonArray>
#include <QTest>

using QindaQt::Test::DesktopNotificationShellDisposition;
using QindaQt::Test::DesktopNotificationShellExpectation;
using QindaQt::Test::DesktopNotificationShellObservation;
using QindaQt::Test::DesktopNotificationShellPhase;
using QindaQt::Test::desktopNotificationShellExpectation;
using QindaQt::Test::desktopNotificationShellSnapshotFixture;
using QindaQt::Test::mutateDesktopNotificationShellPolishFixture;
using QindaQt::Test::validateDesktopNotificationShell;

namespace {

DesktopNotificationShellObservation observation(
    QJsonObject value = desktopNotificationShellSnapshotFixture())
{
    return {QStringLiteral(":1.20"), QStringLiteral(":1.20"), 53, true,
            std::move(value), {}, {}, true, {}, {}};
}

DesktopNotificationShellExpectation beforeExpectation()
{
    return {53, QStringLiteral("WL-0"), {},
            DesktopNotificationShellPhase::ClosedHidden, 0};
}

} // namespace

class DesktopNotificationShellReadinessTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void exactClosedAndOpenStatesPass();
    void ownerAndPidMutationsFailClosed();
    void schemaAndShapeMutationsFailClosed();
    void canonicalFieldsAndGeometryFailClosed();
    void coldPrivacyAndWindowStatesRemainPending();
    void preopenedOrVisibleCenterFailsClosed();
    void outputAndPostStateMustConverge();
    void expectationJoinsOneDockOwnerAndSelectedOutput();
    void coldPublicEnvelopesAreExact();
};

void DesktopNotificationShellReadinessTests::exactClosedAndOpenStatesPass()
{
    const auto before =
        validateDesktopNotificationShell(observation(), beforeExpectation());
    QVERIFY(before.ready());
    QCOMPARE(before.document().value(QStringLiteral("status")),
             QStringLiteral("ok"));

    const DesktopNotificationShellExpectation afterExpectation{
        53, QStringLiteral("WL-0"), {},
        DesktopNotificationShellPhase::OpenVisible, 0};
    const auto after = validateDesktopNotificationShell(
        observation(desktopNotificationShellSnapshotFixture(true, true, true, true, QStringLiteral("WL-0"),
                             QStringLiteral("1"))),
        afterExpectation);
    QVERIFY(after.ready());
}

void DesktopNotificationShellReadinessTests::ownerAndPidMutationsFailClosed()
{
    auto missing = observation();
    missing.owner.clear();
    missing.ownerAfterSnapshot.clear();
    missing.serviceProcessId = 0;
    missing.snapshotReplyValid = false;
    missing.serviceOwnerReplyValid = false;
    missing.serviceOwnerReplyErrorName =
        QStringLiteral("org.freedesktop.DBus.Error.NameHasNoOwner");
    QCOMPARE(validateDesktopNotificationShell(missing, beforeExpectation()).disposition,
             DesktopNotificationShellDisposition::Pending);

    auto transportError = missing;
    transportError.serviceOwnerReplyErrorName =
        QStringLiteral("org.freedesktop.DBus.Error.NoReply");
    QCOMPARE(validateDesktopNotificationShell(transportError,
                                               beforeExpectation()).disposition,
             DesktopNotificationShellDisposition::Invalid);

    auto unexpectedEmpty = missing;
    unexpectedEmpty.serviceOwnerReplyValid = true;
    QCOMPARE(validateDesktopNotificationShell(unexpectedEmpty,
                                               beforeExpectation()).disposition,
             DesktopNotificationShellDisposition::Invalid);

    auto objectPending = observation();
    objectPending.snapshotReplyValid = false;
    objectPending.replyErrorName =
        QStringLiteral("org.freedesktop.DBus.Error.UnknownObject");
    QCOMPARE(validateDesktopNotificationShell(objectPending,
                                               beforeExpectation()).disposition,
             DesktopNotificationShellDisposition::Pending);

    auto malformed = observation();
    malformed.owner = QStringLiteral("org.attacker.Shell");
    malformed.ownerAfterSnapshot = malformed.owner;
    QCOMPARE(validateDesktopNotificationShell(malformed, beforeExpectation()).disposition,
             DesktopNotificationShellDisposition::Invalid);

    auto wrongOwnerPid = observation();
    wrongOwnerPid.serviceProcessId = 54;
    QCOMPARE(validateDesktopNotificationShell(wrongOwnerPid, beforeExpectation()).disposition,
             DesktopNotificationShellDisposition::Invalid);

    auto changedOwner = observation();
    changedOwner.ownerAfterSnapshot = QStringLiteral(":1.21");
    QCOMPARE(validateDesktopNotificationShell(changedOwner, beforeExpectation()).disposition,
             DesktopNotificationShellDisposition::Invalid);

    auto expectedStableOwner = beforeExpectation();
    expectedStableOwner.requiredUniqueOwner = QStringLiteral(":1.19");
    QCOMPARE(validateDesktopNotificationShell(observation(),
                                               expectedStableOwner).disposition,
             DesktopNotificationShellDisposition::Invalid);

    auto wrongSnapshotPid = observation();
    wrongSnapshotPid.snapshot.insert(QStringLiteral("shellPid"), QStringLiteral("54"));
    QCOMPARE(validateDesktopNotificationShell(wrongSnapshotPid, beforeExpectation()).disposition,
             DesktopNotificationShellDisposition::Invalid);
}

void DesktopNotificationShellReadinessTests::schemaAndShapeMutationsFailClosed()
{
    auto empty = observation({});
    QCOMPARE(validateDesktopNotificationShell(empty, beforeExpectation()).disposition,
             DesktopNotificationShellDisposition::Invalid);
    for (const auto &mutation : {QStringLiteral("schema"), QStringLiteral("pid"),
                                 QStringLiteral("tokens"),
                                 QStringLiteral("taskList"),
                                 QStringLiteral("quieting"),
                                 QStringLiteral("panelApplets"),
                                 QStringLiteral("presentation"),
                                 QStringLiteral("counter")}) {
        auto changed = observation();
        if (mutation == QStringLiteral("schema")) {
            changed.snapshot.insert(QStringLiteral("schemaVersion"), 2);
        } else if (mutation == QStringLiteral("pid")) {
            changed.snapshot.insert(QStringLiteral("shellPid"), 53);
        } else if (mutation == QStringLiteral("tokens")) {
            QJsonObject tokens =
                changed.snapshot.value(QStringLiteral("tokens")).toObject();
            tokens.insert(QStringLiteral("ready"), false);
            changed.snapshot.insert(QStringLiteral("tokens"), tokens);
        } else if (mutation == QStringLiteral("taskList")) {
            auto taskList = changed.snapshot.value(QStringLiteral("taskList")).toObject();
            taskList.insert(QStringLiteral("phase"), QStringLiteral("foreign"));
            changed.snapshot.insert(QStringLiteral("taskList"), taskList);
        } else if (mutation == QStringLiteral("quieting")
                   || mutation == QStringLiteral("panelApplets")) {
            mutateDesktopNotificationShellPolishFixture(changed.snapshot, mutation);
        } else if (mutation == QStringLiteral("presentation")) {
            changed.snapshot.insert(QStringLiteral("presentation"), QJsonArray{});
        } else {
            QJsonObject observations =
                changed.snapshot.value(QStringLiteral("observations")).toObject();
            observations.insert(QStringLiteral("centerOpenedCount"), -1);
            changed.snapshot.insert(QStringLiteral("observations"), observations);
        }
        QCOMPARE(validateDesktopNotificationShell(changed, beforeExpectation()).disposition,
                 mutation == QStringLiteral("quieting")
                     ? DesktopNotificationShellDisposition::Pending
                     : DesktopNotificationShellDisposition::Invalid);
    }

    for (const auto &[field, value] : {
             std::pair{QStringLiteral("qstRevision"), QJsonValue(2)},
             std::pair{QStringLiteral("generation"),
                       QJsonValue(QStringLiteral("0"))},
             std::pair{QStringLiteral("sourceThemeId"),
                       QJsonValue(QString{})},
             std::pair{QStringLiteral("backgroundBase"),
                       QJsonValue(QStringLiteral("#ABCDEF"))}}) {
        auto changed = observation();
        QJsonObject tokens =
            changed.snapshot.value(QStringLiteral("tokens")).toObject();
        tokens.insert(field, value);
        changed.snapshot.insert(QStringLiteral("tokens"), tokens);
        QCOMPARE(validateDesktopNotificationShell(changed,
                                                   beforeExpectation()).disposition,
                 DesktopNotificationShellDisposition::Invalid);
    }
}

void DesktopNotificationShellReadinessTests::
    canonicalFieldsAndGeometryFailClosed()
{
    for (const QJsonValue &value : {
             QJsonValue(53), QJsonValue(QStringLiteral("053")),
             QJsonValue(QStringLiteral("0")), QJsonValue(true)}) {
        auto changed = observation();
        changed.snapshot.insert(QStringLiteral("shellPid"), value);
        QCOMPARE(validateDesktopNotificationShell(changed,
                                                   beforeExpectation()).disposition,
                 DesktopNotificationShellDisposition::Invalid);
    }
    for (const QJsonValue &value : {
             QJsonValue(0), QJsonValue(QStringLiteral("01")),
             QJsonValue(QStringLiteral("-1")), QJsonValue(true)}) {
        auto changed = observation();
        QJsonObject observations =
            changed.snapshot.value(QStringLiteral("observations")).toObject();
        observations.insert(QStringLiteral("centerOpenedCount"), value);
        changed.snapshot.insert(QStringLiteral("observations"), observations);
        QCOMPARE(validateDesktopNotificationShell(changed,
                                                   beforeExpectation()).disposition,
                 DesktopNotificationShellDisposition::Invalid);
    }
    for (const auto &mutation : {QStringLiteral("missing-x"),
                                 QStringLiteral("boolean-x"),
                                 QStringLiteral("zero-width")}) {
        auto changed = observation();
        QJsonObject windows =
            changed.snapshot.value(QStringLiteral("windows")).toObject();
        QJsonObject center = windows.value(QStringLiteral("center")).toObject();
        QJsonObject geometry = center.value(QStringLiteral("geometry")).toObject();
        if (mutation == QStringLiteral("missing-x")) {
            geometry.remove(QStringLiteral("x"));
        } else if (mutation == QStringLiteral("boolean-x")) {
            geometry.insert(QStringLiteral("x"), true);
        } else {
            geometry.insert(QStringLiteral("width"), 0);
        }
        center.insert(QStringLiteral("geometry"), geometry);
        windows.insert(QStringLiteral("center"), center);
        changed.snapshot.insert(QStringLiteral("windows"), windows);
        QCOMPARE(validateDesktopNotificationShell(changed,
                                                   beforeExpectation()).disposition,
                 DesktopNotificationShellDisposition::Invalid);
    }
}

void DesktopNotificationShellReadinessTests::coldPrivacyAndWindowStatesRemainPending()
{
    const auto privacy = validateDesktopNotificationShell(
        observation(desktopNotificationShellSnapshotFixture(false)), beforeExpectation());
    QCOMPARE(privacy.disposition, DesktopNotificationShellDisposition::Pending);
    QCOMPARE(privacy.document().value(QStringLiteral("evidence"))
                 .toObject()
                 .value(QStringLiteral("presentation"))
                 .toObject()
                 .value(QStringLiteral("privatePresentationAllowed")),
             QJsonValue(false));
    QCOMPARE(validateDesktopNotificationShell(
                 observation(desktopNotificationShellSnapshotFixture(true, false, false)), beforeExpectation()).disposition,
             DesktopNotificationShellDisposition::Pending);
}

void DesktopNotificationShellReadinessTests::preopenedOrVisibleCenterFailsClosed()
{
    QCOMPARE(validateDesktopNotificationShell(
                 observation(desktopNotificationShellSnapshotFixture(true, true, true, true)), beforeExpectation()).disposition,
             DesktopNotificationShellDisposition::Invalid);
    QCOMPARE(validateDesktopNotificationShell(
                 observation(desktopNotificationShellSnapshotFixture(true, false, true, true)), beforeExpectation()).disposition,
             DesktopNotificationShellDisposition::Invalid);
}

void DesktopNotificationShellReadinessTests::outputAndPostStateMustConverge()
{
    QCOMPARE(validateDesktopNotificationShell(
                 observation(desktopNotificationShellSnapshotFixture(true, false, true, false,
                                      QStringLiteral("WL-1"))),
                 beforeExpectation()).disposition,
             DesktopNotificationShellDisposition::Pending);

    const DesktopNotificationShellExpectation afterExpectation{
        53, QStringLiteral("WL-0"), {},
        DesktopNotificationShellPhase::OpenVisible, 4};
    for (const auto &state : {
             desktopNotificationShellSnapshotFixture(true, false, true, true, QStringLiteral("WL-0"),
                      QStringLiteral("5")),
             desktopNotificationShellSnapshotFixture(true, true, true, false, QStringLiteral("WL-0"),
                      QStringLiteral("5")),
             desktopNotificationShellSnapshotFixture(true, true, true, true, QStringLiteral("WL-0"),
                      QStringLiteral("4")),
         }) {
        QCOMPARE(validateDesktopNotificationShell(observation(state),
                                                   afterExpectation).disposition,
                 DesktopNotificationShellDisposition::Pending);
    }
}

void DesktopNotificationShellReadinessTests::
    expectationJoinsOneDockOwnerAndSelectedOutput()
{
    const QJsonObject surfaces{
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("surfaces"),
         QJsonArray{
             QJsonObject{{QStringLiteral("scope"), QStringLiteral("dock")},
                         {QStringLiteral("processId"), QStringLiteral("53")},
                         {QStringLiteral("mapped"), true},
                         {QStringLiteral("committed"), true},
                         {QStringLiteral("outputName"), QStringLiteral("WL-0")},
                         {QStringLiteral("desiredOutputName"), QStringLiteral("WL-0")},
                         {QStringLiteral("geometry"),
                          QJsonObject{{QStringLiteral("x"), 0},
                                      {QStringLiteral("y"), 0},
                                      {QStringLiteral("width"), 1920},
                                      {QStringLiteral("height"), 30}}}},
             QJsonObject{{QStringLiteral("scope"), QStringLiteral("dock")},
                         {QStringLiteral("processId"), QStringLiteral("53")},
                         {QStringLiteral("mapped"), true},
                         {QStringLiteral("committed"), true},
                         {QStringLiteral("outputName"), QStringLiteral("WL-1")},
                         {QStringLiteral("desiredOutputName"), QStringLiteral("WL-1")},
                         {QStringLiteral("geometry"),
                          QJsonObject{{QStringLiteral("x"), 1920},
                                      {QStringLiteral("y"), 0},
                                      {QStringLiteral("width"), 1920},
                                      {QStringLiteral("height"), 30}}}},
         }},
    };
    const QJsonObject outputs{
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("outputs"),
         QJsonArray{
             QJsonObject{{QStringLiteral("name"), QStringLiteral("WL-1")}},
             QJsonObject{{QStringLiteral("name"), QStringLiteral("WL-0")}},
         }},
    };
    QString error;
    const auto expectation = desktopNotificationShellExpectation(
        surfaces, outputs, {}, DesktopNotificationShellPhase::ClosedHidden, 0,
        &error);
    QVERIFY2(expectation.ready(), qPrintable(error));
    QCOMPARE(expectation.expectation->dockProcessId, 53);
    QCOMPARE(expectation.expectation->outputName, QStringLiteral("WL-1"));

    QJsonObject conflicting = surfaces;
    QJsonArray docks = conflicting.value(QStringLiteral("surfaces")).toArray();
    QJsonObject second = docks.at(1).toObject();
    second.insert(QStringLiteral("processId"), QStringLiteral("54"));
    docks.replace(1, second);
    conflicting.insert(QStringLiteral("surfaces"), docks);
    QCOMPARE(desktopNotificationShellExpectation(
                 conflicting, outputs, {},
                 DesktopNotificationShellPhase::ClosedHidden, 0,
                 &error).disposition,
             DesktopNotificationShellDisposition::Invalid);

    QJsonObject zeroSized = surfaces;
    docks = zeroSized.value(QStringLiteral("surfaces")).toArray();
    for (qsizetype index = 0; index < docks.size(); ++index) {
        QJsonObject dock = docks.at(index).toObject();
        dock.insert(QStringLiteral("geometry"),
                    QJsonObject{{QStringLiteral("x"), 0},
                                {QStringLiteral("y"), 0},
                                {QStringLiteral("width"), 0},
                                {QStringLiteral("height"), 0}});
        docks.replace(index, dock);
    }
    zeroSized.insert(QStringLiteral("surfaces"), docks);
    QCOMPARE(desktopNotificationShellExpectation(
                 zeroSized, outputs, {},
                 DesktopNotificationShellPhase::ClosedHidden, 0,
                 &error).disposition,
             DesktopNotificationShellDisposition::Pending);

    QJsonObject malformed = surfaces;
    docks = malformed.value(QStringLiteral("surfaces")).toArray();
    QJsonObject dock = docks.at(0).toObject();
    dock.insert(QStringLiteral("geometry"),
                QJsonObject{{QStringLiteral("width"), 1920},
                            {QStringLiteral("height"), 30}});
    docks.replace(0, dock);
    malformed.insert(QStringLiteral("surfaces"), docks);
    QCOMPARE(desktopNotificationShellExpectation(
                 malformed, outputs, {},
                 DesktopNotificationShellPhase::ClosedHidden, 0,
                 &error).disposition,
             DesktopNotificationShellDisposition::Invalid);

    for (const auto &mutation : {QStringLiteral("unmapped"),
                                 QStringLiteral("unconverged")}) {
        QJsonObject unsettled = surfaces;
        docks = unsettled.value(QStringLiteral("surfaces")).toArray();
        dock = docks.at(0).toObject();
        if (mutation == QStringLiteral("unmapped")) {
            dock.insert(QStringLiteral("mapped"), false);
        } else {
            dock.insert(QStringLiteral("desiredOutputName"),
                        QStringLiteral("WL-1"));
        }
        docks.replace(0, dock);
        unsettled.insert(QStringLiteral("surfaces"), docks);
        QCOMPARE(desktopNotificationShellExpectation(
                     unsettled, outputs, {},
                     DesktopNotificationShellPhase::ClosedHidden, 0,
                     &error).disposition,
                 DesktopNotificationShellDisposition::Pending);
    }

    QJsonObject foreign = surfaces;
    docks = foreign.value(QStringLiteral("surfaces")).toArray();
    dock = docks.at(0).toObject();
    dock.insert(QStringLiteral("outputName"), QStringLiteral("WL-9"));
    docks.replace(0, dock);
    foreign.insert(QStringLiteral("surfaces"), docks);
    QCOMPARE(desktopNotificationShellExpectation(
                 foreign, outputs, {},
                 DesktopNotificationShellPhase::ClosedHidden, 0,
                 &error).disposition,
             DesktopNotificationShellDisposition::Invalid);

    QJsonObject nonCanonicalOwner = zeroSized;
    docks = nonCanonicalOwner.value(QStringLiteral("surfaces")).toArray();
    dock = docks.at(0).toObject();
    dock.insert(QStringLiteral("processId"), QStringLiteral("053"));
    docks.replace(0, dock);
    nonCanonicalOwner.insert(QStringLiteral("surfaces"), docks);
    QCOMPARE(desktopNotificationShellExpectation(
                 nonCanonicalOwner, outputs, {},
                 DesktopNotificationShellPhase::ClosedHidden, 0,
                 &error).disposition,
             DesktopNotificationShellDisposition::Invalid);

    for (const auto &mutation : {QStringLiteral("bad-surfaces"),
                                 QStringLiteral("bad-outputs")}) {
        QJsonObject changedSurfaces = surfaces;
        QJsonObject changedOutputs = outputs;
        changedSurfaces.insert(
            QStringLiteral("status"),
            mutation == QStringLiteral("bad-surfaces")
                ? QStringLiteral("error") : QStringLiteral("unavailable"));
        changedOutputs.insert(
            QStringLiteral("status"),
            mutation == QStringLiteral("bad-outputs")
                ? QStringLiteral("error") : QStringLiteral("unavailable"));
        QCOMPARE(desktopNotificationShellExpectation(
                     changedSurfaces, changedOutputs, {},
                     DesktopNotificationShellPhase::ClosedHidden, 0,
                     &error).disposition,
                 DesktopNotificationShellDisposition::Invalid);
    }

}

void DesktopNotificationShellReadinessTests::coldPublicEnvelopesAreExact()
{
    const QJsonObject cold{
        {QStringLiteral("status"), QStringLiteral("unavailable")},
        {QStringLiteral("failure"),
         QJsonObject{{QStringLiteral("code"), QStringLiteral("service-not-ready")},
                     {QStringLiteral("message"), QStringLiteral("not sampled")}}},
    };
    const QJsonObject surfaces{
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("surfaces"), QJsonArray{}},
    };
    const QJsonObject outputs{
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("outputs"), QJsonArray{}},
    };
    QString error;
    QCOMPARE(desktopNotificationShellExpectation(
                 cold, outputs, {},
                 DesktopNotificationShellPhase::ClosedHidden, 0,
                 &error).disposition,
             DesktopNotificationShellDisposition::Pending);
    QCOMPARE(desktopNotificationShellExpectation(
                 surfaces, cold, {},
                 DesktopNotificationShellPhase::ClosedHidden, 0,
                 &error).disposition,
             DesktopNotificationShellDisposition::Pending);
    const QJsonObject malformedSurfaces{
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("surfaces"), QStringLiteral("not-an-array")},
    };
    const QJsonObject malformedOutputs{
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("outputs"), QStringLiteral("not-an-array")},
    };
    QCOMPARE(desktopNotificationShellExpectation(
                 cold, malformedOutputs, {},
                 DesktopNotificationShellPhase::ClosedHidden, 0,
                 &error).disposition,
             DesktopNotificationShellDisposition::Invalid);
    QCOMPARE(desktopNotificationShellExpectation(
                 malformedSurfaces, cold, {},
                 DesktopNotificationShellPhase::ClosedHidden, 0,
                 &error).disposition,
             DesktopNotificationShellDisposition::Invalid);
    for (const QJsonObject &invalidCold : {
             QJsonObject{{QStringLiteral("status"), QStringLiteral("unavailable")}},
             QJsonObject{
                 {QStringLiteral("status"), QStringLiteral("unavailable")},
                 {QStringLiteral("failure"),
                  QJsonObject{{QStringLiteral("code"), QStringLiteral("dbus-call-failed")},
                              {QStringLiteral("message"), QStringLiteral("failed")}}}},
         }) {
        QCOMPARE(desktopNotificationShellExpectation(
                     invalidCold, outputs, {},
                     DesktopNotificationShellPhase::ClosedHidden, 0,
                     &error).disposition,
                 DesktopNotificationShellDisposition::Invalid);
    }
}

QTEST_GUILESS_MAIN(DesktopNotificationShellReadinessTests)

#include "tst_desktopnotificationshellreadiness.moc"
