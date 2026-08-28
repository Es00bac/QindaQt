// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/compositor/shellvisibilitysnapshot.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <algorithm>

using namespace QindaQt::Compositor;

namespace {

ShellVisibilitySnapshotCandidate validCandidate()
{
    return {
        .scope = {QStringLiteral("workspace-1"), QStringLiteral("activity-1")},
        .outputs = {{QStringLiteral("DP-2"), QRect(1920, 0, 2560, 1440), 1.5},
                    {QStringLiteral("DP-1"), QRect(0, 0, 1920, 1080), 1.0}},
        .windows = {{.id = QStringLiteral("window-b"),
                     .outputId = QStringLiteral("DP-2"),
                     .frameGeometry = QRect(2000, 40, 800, 600),
                     .workspaceIds = {},
                     .activityIds = {},
                     .onAllWorkspaces = true,
                     .maximized = true},
                    {.id = QStringLiteral("window-a"),
                     .outputId = QStringLiteral("DP-1"),
                     .frameGeometry = QRect(10, 20, 900, 700),
                     .workspaceIds = {QStringLiteral("workspace-2"),
                                      QStringLiteral("workspace-1")},
                     .activityIds = {QStringLiteral("activity-2"),
                                     QStringLiteral("activity-1")},
                     .active = true}},
    };
}

QJsonObject parse(const QByteArray &payload)
{
    const auto document = QJsonDocument::fromJson(payload);
    return document.isObject() ? document.object() : QJsonObject{};
}

} // namespace

class ShellVisibilitySnapshotTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void publishesExactCanonicalWireShape();
    void rejectsUnsafeAndAmbiguousGenerationsAtomically();
    void advancesOnlyForChangedValidGenerations();
    void bindsRevisionToOutputGeneration();
    void enforcesConsumerWireLimits();
    void publishesUnavailableAndRecoversAtANewRevision();
    void revisionExhaustionConvergesToUnavailable();
};

void ShellVisibilitySnapshotTest::publishesExactCanonicalWireShape()
{
    ShellVisibilitySnapshotStore store(QStringLiteral("epoch-a"));
    QCOMPARE(store.publish(validCandidate()), ShellVisibilityPublishResult::Published);
    QCOMPARE(store.revision(), quint64(1));

    const auto snapshot = parse(store.snapshotJson());
    QCOMPARE(snapshot.value(QStringLiteral("status")).toString(), QStringLiteral("ok"));
    QCOMPARE(snapshot.value(QStringLiteral("schemaVersion")).toInt(), 1);
    QCOMPARE(snapshot.value(QStringLiteral("epoch")).toString(),
             QStringLiteral("epoch-a"));
    QCOMPARE(snapshot.value(QStringLiteral("revision")).toString(), QStringLiteral("1"));
    QCOMPARE(snapshot.value(QStringLiteral("outputGeneration")).toString(),
             QStringLiteral("1"));
    const auto scope = snapshot.value(QStringLiteral("scope")).toObject();
    QCOMPARE(scope.value(QStringLiteral("workspaceId")).toString(),
             QStringLiteral("workspace-1"));
    QCOMPARE(scope.value(QStringLiteral("activityId")).toString(),
             QStringLiteral("activity-1"));

    const auto outputs = snapshot.value(QStringLiteral("outputs")).toArray();
    QCOMPARE(outputs.size(), 2);
    QCOMPARE(outputs.at(0).toObject().value(QStringLiteral("id")).toString(),
             QStringLiteral("DP-1"));
    QCOMPARE(outputs.at(1).toObject().value(QStringLiteral("scale")).toDouble(), 1.5);

    const auto windows = snapshot.value(QStringLiteral("windows")).toArray();
    QCOMPARE(windows.size(), 2);
    const auto first = windows.at(0).toObject();
    QCOMPARE(first.value(QStringLiteral("id")).toString(), QStringLiteral("window-a"));
    QCOMPARE(first.value(QStringLiteral("outputId")).toString(), QStringLiteral("DP-1"));
    QCOMPARE(first.value(QStringLiteral("workspaceIds")).toArray(),
             QJsonArray({QStringLiteral("workspace-1"), QStringLiteral("workspace-2")}));
    QCOMPARE(first.value(QStringLiteral("activityIds")).toArray(),
             QJsonArray({QStringLiteral("activity-1"), QStringLiteral("activity-2")}));
    QVERIFY(first.value(QStringLiteral("active")).toBool());
    QVERIFY(!first.value(QStringLiteral("minimized")).toBool());
    const auto second = windows.at(1).toObject();
    QVERIFY(second.value(QStringLiteral("onAllWorkspaces")).toBool());
    QVERIFY(second.value(QStringLiteral("workspaceIds")).toArray().isEmpty());
    QVERIFY(second.value(QStringLiteral("maximized")).toBool());
}

void ShellVisibilitySnapshotTest::bindsRevisionToOutputGeneration()
{
    ShellVisibilitySnapshotStore store(QStringLiteral("epoch-a"));
    auto candidate = validCandidate();
    QCOMPARE(store.publish(candidate), ShellVisibilityPublishResult::Published);
    candidate.outputGeneration = 2;
    QCOMPARE(store.publish(candidate), ShellVisibilityPublishResult::Published);
    QCOMPARE(store.revision(), quint64(2));
    QCOMPARE(parse(store.snapshotJson()).value(QStringLiteral("outputGeneration")).toString(),
             QStringLiteral("2"));
    const auto retained = store.snapshotJson();
    candidate.outputGeneration = 0;
    QCOMPARE(store.publish(candidate), ShellVisibilityPublishResult::Rejected);
    QCOMPARE(store.snapshotJson(), retained);
}

void ShellVisibilitySnapshotTest::rejectsUnsafeAndAmbiguousGenerationsAtomically()
{
    ShellVisibilitySnapshotStore store(QStringLiteral("epoch-a"));
    auto candidate = validCandidate();
    QCOMPARE(store.publish(candidate), ShellVisibilityPublishResult::Published);
    const auto retained = store.snapshotJson();
    candidate.outputs.append({QStringLiteral("duplicate"), QRect(0, 0, 10, 10), 1.0});
    candidate.outputs.append({QStringLiteral("duplicate"), QRect(10, 0, 10, 10), 1.0});
    candidate.outputs.append({QStringLiteral("bad-scale"), QRect(0, 0, 10, 10), 0.0});
    candidate.windows.append({.id = QStringLiteral("ambiguous-output"),
                              .outputId = QStringLiteral("duplicate"),
                              .frameGeometry = QRect(0, 0, 5, 5),
                              .workspaceIds = {QStringLiteral("workspace-1")}});
    candidate.windows.append({.id = QStringLiteral("duplicate-window"),
                              .outputId = QStringLiteral("DP-1"),
                              .frameGeometry = QRect(20, 20, 10, 10),
                              .workspaceIds = {QStringLiteral("workspace-1")}});
    candidate.windows.append({.id = QStringLiteral("duplicate-window"),
                              .outputId = QStringLiteral("DP-1"),
                              .frameGeometry = QRect(40, 40, 10, 10),
                              .workspaceIds = {QStringLiteral("workspace-1")}});
    candidate.windows.append({.id = QStringLiteral("outside-output"),
                              .outputId = QStringLiteral("DP-1"),
                              .frameGeometry = QRect(9000, 9000, 10, 10),
                              .workspaceIds = {QStringLiteral("workspace-1")}});

    QString error;
    QCOMPARE(store.publish(candidate, &error), ShellVisibilityPublishResult::Rejected);
    QVERIFY(!error.isEmpty());
    QCOMPARE(store.revision(), quint64(1));
    QCOMPARE(store.snapshotJson(), retained);
}

void ShellVisibilitySnapshotTest::advancesOnlyForChangedValidGenerations()
{
    ShellVisibilitySnapshotStore store(QStringLiteral("epoch-a"));
    auto candidate = validCandidate();
    QCOMPARE(store.publish(candidate), ShellVisibilityPublishResult::Published);
    const auto firstPayload = store.snapshotJson();

    std::reverse(candidate.outputs.begin(), candidate.outputs.end());
    std::reverse(candidate.windows.begin(), candidate.windows.end());
    QCOMPARE(store.publish(candidate), ShellVisibilityPublishResult::Unchanged);
    QCOMPARE(store.revision(), quint64(1));
    QCOMPARE(store.snapshotJson(), firstPayload);

    candidate.scope.workspaceId.clear();
    QString error;
    QCOMPARE(store.publish(candidate, &error), ShellVisibilityPublishResult::Rejected);
    QVERIFY(!error.isEmpty());
    QCOMPARE(store.revision(), quint64(1));
    QCOMPARE(store.snapshotJson(), firstPayload);

    candidate = validCandidate();
    candidate.windows[0].hidden = true;
    QCOMPARE(store.publish(candidate), ShellVisibilityPublishResult::Published);
    QCOMPARE(store.revision(), quint64(2));
    QCOMPARE(parse(store.snapshotJson()).value(QStringLiteral("revision")).toString(),
             QStringLiteral("2"));
}

void ShellVisibilitySnapshotTest::enforcesConsumerWireLimits()
{
    ShellVisibilitySnapshotStore store(QStringLiteral("epoch-a"));
    auto candidate = validCandidate();
    for (qsizetype index = candidate.outputs.size();
         index <= ShellVisibilityWireLimits::MaxOutputs; ++index) {
        candidate.outputs.append({QStringLiteral("output-%1").arg(index),
                                  QRect(int(index) * 10, 0, 10, 10), 1.0});
    }
    QCOMPARE(store.publish(candidate), ShellVisibilityPublishResult::Rejected);
    QCOMPARE(store.revision(), quint64(0));

    candidate = validCandidate();
    candidate.windows[0].workspaceIds.clear();
    for (qsizetype index = 0;
         index <= ShellVisibilityWireLimits::MaxScopeMemberships; ++index) {
        candidate.windows[0].workspaceIds.append(
            QStringLiteral("workspace-%1").arg(index));
    }
    candidate.windows[0].onAllWorkspaces = false;
    QCOMPARE(store.publish(candidate), ShellVisibilityPublishResult::Rejected);

    candidate = validCandidate();
    candidate.outputs[0].scale = ShellVisibilityWireLimits::MaxOutputScale + 0.5;
    QCOMPARE(store.publish(candidate), ShellVisibilityPublishResult::Rejected);

    candidate = validCandidate();
    candidate.windows[0].id =
        QString(ShellVisibilityWireLimits::MaxIdentifierCharacters + 1,
                QLatin1Char('x'));
    QCOMPARE(store.publish(candidate), ShellVisibilityPublishResult::Rejected);

    candidate = validCandidate();
    QStringList largeMemberships;
    largeMemberships.reserve(ShellVisibilityWireLimits::MaxScopeMemberships);
    for (qsizetype index = 0;
         index < ShellVisibilityWireLimits::MaxScopeMemberships; ++index) {
        const QString prefix = QStringLiteral("membership-%1-").arg(index);
        largeMemberships.append(
            prefix + QString(ShellVisibilityWireLimits::MaxIdentifierCharacters
                                 - prefix.size(),
                             QLatin1Char('x')));
    }
    candidate.windows.clear();
    for (int index = 0; index < 20; ++index) {
        candidate.windows.append({.id = QStringLiteral("large-window-%1").arg(index),
                                  .outputId = QStringLiteral("DP-1"),
                                  .frameGeometry = QRect(0, 0, 10, 10),
                                  .workspaceIds = largeMemberships,
                                  .activityIds = largeMemberships});
    }
    QCOMPARE(store.publish(candidate), ShellVisibilityPublishResult::Rejected);
    QCOMPARE(store.revision(), quint64(0));
    QCOMPARE(parse(store.snapshotJson()).value(QStringLiteral("status")).toString(),
             QStringLiteral("unavailable"));
}

void ShellVisibilitySnapshotTest::publishesUnavailableAndRecoversAtANewRevision()
{
    ShellVisibilitySnapshotStore store(QStringLiteral("epoch-a"));
    const auto candidate = validCandidate();
    QCOMPARE(store.publish(candidate), ShellVisibilityPublishResult::Published);
    QVERIFY(store.available());
    QVERIFY(store.markUnavailable(QStringLiteral("snapshot-invalid"),
                                  QStringLiteral("inventory is temporarily invalid")));
    QVERIFY(!store.available());
    auto unavailable = parse(store.snapshotJson());
    QCOMPARE(unavailable.value(QStringLiteral("status")).toString(),
             QStringLiteral("unavailable"));
    QCOMPARE(unavailable.value(QStringLiteral("epoch")).toString(),
             QStringLiteral("epoch-a"));
    QCOMPARE(unavailable.value(QStringLiteral("revision")).toString(),
             QStringLiteral("1"));
    QVERIFY(!store.markUnavailable(QStringLiteral("snapshot-invalid"),
                                   QStringLiteral("still invalid")));

    QCOMPARE(store.publish(candidate), ShellVisibilityPublishResult::Published);
    QVERIFY(store.available());
    QCOMPARE(store.revision(), quint64(2));
    QCOMPARE(parse(store.snapshotJson()).value(QStringLiteral("status")).toString(),
             QStringLiteral("ok"));
}

void ShellVisibilitySnapshotTest::revisionExhaustionConvergesToUnavailable()
{
    const auto maximum = std::numeric_limits<quint64>::max();
    ShellVisibilitySnapshotStore store(
        QStringLiteral("epoch-a"), ShellVisibilityRevisionSeed{maximum - 1});
    auto candidate = validCandidate();
    QCOMPARE(store.publish(candidate), ShellVisibilityPublishResult::Published);
    QCOMPARE(store.revision(), maximum);
    QVERIFY(store.available());

    candidate.scope.workspaceId = QStringLiteral("workspace-2");
    QCOMPARE(store.publish(candidate),
             ShellVisibilityPublishResult::RevisionExhausted);
    QVERIFY(store.markUnavailable(QStringLiteral("revision-exhausted"),
                                  QStringLiteral("revision is exhausted")));
    QVERIFY(!store.available());
    const auto unavailable = parse(store.snapshotJson());
    QCOMPARE(unavailable.value(QStringLiteral("status")).toString(),
             QStringLiteral("unavailable"));
    QCOMPARE(unavailable.value(QStringLiteral("revision")).toString(),
             QString::number(maximum));
    QCOMPARE(store.publish(candidate),
             ShellVisibilityPublishResult::RevisionExhausted);
    QVERIFY(!store.markUnavailable(QStringLiteral("revision-exhausted"),
                                   QStringLiteral("still exhausted")));
}

QTEST_APPLESS_MAIN(ShellVisibilitySnapshotTest)

#include "tst_shellvisibilitysnapshot.moc"
