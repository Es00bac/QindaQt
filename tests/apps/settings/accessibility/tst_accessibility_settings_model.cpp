// SPDX-License-Identifier: GPL-3.0-or-later
#include "accessibility_settings_test_support.h"

#include <qindaqt/apps/settings_accessibility/accessibility_settings_model.h>
#include <qindaqt/services/settings_client/settings_client.h>

#include <QtTest>

using namespace QindaQt::Apps::SettingsAccessibility;
using namespace QindaQt::Apps::SettingsAccessibility::TestSupport;
using QindaQt::Services::SettingsClient::ClientTiming;
using QindaQt::Services::SettingsClient::SettingsClient;

namespace {
[[nodiscard]] ClientTiming fastTiming(int requestTimeoutMilliseconds = 100)
{
    return {.requestTimeoutMilliseconds = requestTimeoutMilliseconds,
            .debounceMilliseconds = 0,
            .retryMilliseconds = {10}};
}

[[nodiscard]] QString operationKey(const FakeSettingsTransport::CommitRequest &commit)
{
    return commit.operations.constFirst().toMap()
        .value(QLatin1StringView(WireContract::FieldKey)).toString();
}

[[nodiscard]] QVariant operationValue(const FakeSettingsTransport::CommitRequest &commit)
{
    return commit.operations.constFirst().toMap()
        .value(QLatin1StringView(WireContract::FieldValue));
}
} // namespace

class AccessibilitySettingsModelTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void loadingThenReadyScopesExactlyFourKeys();
    void draftSettersGateAndRevertRestores();
    void applySequencesPerKeyCommitsInOrder();
    void malformedRefreshFailsClosed();
    void conflictStopsSequenceAndRequiresExplicitReapply();
    void uncertainWriteIsNeverReplayed();
    void replacementDuringSequenceAbortsWithoutReplay();
};

void AccessibilitySettingsModelTest::loadingThenReadyScopesExactlyFourKeys()
{
    FakeSettingsTransport transport;
    SettingsClient client(transport, AccessibilityKeys::scopedKeys(), fastTiming());
    AccessibilitySettingsModel model(client);
    QVERIFY(model.loading());
    QVERIFY(!model.canEdit());
    QVERIFY(!model.draftDirty());
    QVERIFY(client.start());

    QVariantMap values = defaultValues();
    values[QLatin1String(AccessibilityKeys::HighContrast)] = true;
    // An integral JSON number is a valid scale on the wire.
    values[QLatin1String(AccessibilityKeys::TextScale)] = 2;
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.30"));
    QTRY_VERIFY(!transport.snapshots.isEmpty());
    const auto request = transport.snapshots.constFirst();
    // AGENT-GUARD: accessibility.screenReader has no consumer and must never
    // be scoped, read, or written by this route.
    QCOMPARE(request.keys.size(), 4);
    QVERIFY(request.keys.contains(QLatin1String(AccessibilityKeys::HighContrast)));
    QVERIFY(request.keys.contains(QLatin1String(AccessibilityKeys::ReducedMotion)));
    QVERIFY(request.keys.contains(QLatin1String(AccessibilityKeys::ReducedTransparency)));
    QVERIFY(request.keys.contains(QLatin1String(AccessibilityKeys::TextScale)));
    for (const QString &key : request.keys) {
        QVERIFY(!key.contains(QStringLiteral("screenReader")));
    }
    QVERIFY(answerSnapshot(transport, QStringLiteral("epoch-a"), 3, values));

    QTRY_VERIFY(model.ready());
    QVERIFY(model.canEdit());
    QVERIFY(!model.draftDirty());
    QVERIFY(!model.applyAvailable());
    QVERIFY(model.highContrast());
    QVERIFY(model.draftHighContrast());
    QVERIFY(!model.reducedMotion());
    QVERIFY(!model.reducedTransparency());
    QCOMPARE(model.textScale(), 2.0);
    QCOMPARE(model.draftTextScale(), 2.0);
    QCOMPARE(model.minimumTextScale(), 0.5);
    QCOMPARE(model.maximumTextScale(), 3.0);
    QCOMPARE(model.defaultTextScale(), 1.0);
    QVERIFY(model.statusText().isEmpty());
    QVERIFY(model.errorText().isEmpty());
}

void AccessibilitySettingsModelTest::draftSettersGateAndRevertRestores()
{
    FakeSettingsTransport transport;
    SettingsClient client(transport, AccessibilityKeys::scopedKeys(), fastTiming());
    AccessibilitySettingsModel model(client);
    // No baseline: every setter is refused rather than silently buffered.
    QVERIFY(!model.setDraftHighContrast(true));
    QVERIFY(!model.setDraftTextScale(1.5));
    QVERIFY(client.start());
    QVERIFY(establishBaseline(transport, QStringLiteral(":1.31"),
                              QStringLiteral("epoch-a"), 4, defaultValues()));
    QTRY_VERIFY(model.ready());

    QVERIFY(!model.setDraftTextScale(3.5));
    QVERIFY(!model.textScaleError().isEmpty());
    QVERIFY(!model.draftDirty());
    QVERIFY(!model.setDraftTextScale(0.25));
    QVERIFY(model.setDraftTextScale(1.5));
    QVERIFY(model.textScaleError().isEmpty());
    QVERIFY(model.setDraftReducedMotion(true));
    QVERIFY(model.draftDirty());
    QVERIFY(model.applyAvailable());
    QVERIFY(model.statusText().contains(QStringLiteral("not yet applied")));
    QVERIFY(!model.reducedMotion());
    QCOMPARE(model.textScale(), 1.0);

    QVERIFY(model.revertDraft());
    QVERIFY(!model.draftDirty());
    QVERIFY(!model.draftReducedMotion());
    QCOMPARE(model.draftTextScale(), 1.0);
    QVERIFY(!model.applyAvailable());
    // Revert without dirt is refused, not silently ignored.
    QVERIFY(!model.revertDraft());
    QCOMPARE(transport.commits.size(), 0);
}

void AccessibilitySettingsModelTest::applySequencesPerKeyCommitsInOrder()
{
    FakeSettingsTransport transport;
    SettingsClient client(transport, AccessibilityKeys::scopedKeys(), fastTiming());
    AccessibilitySettingsModel model(client);
    QVERIFY(client.start());
    QVERIFY(establishBaseline(transport, QStringLiteral(":1.32"),
                              QStringLiteral("epoch-a"), 7, defaultValues()));
    QTRY_VERIFY(model.ready());

    QVERIFY(model.setDraftTextScale(1.5));
    QVERIFY(model.setDraftHighContrast(true));
    QVERIFY(model.applyDraft());
    QVERIFY(model.saving());
    QVERIFY(!model.canEdit());

    // The first changed key in scoped order commits alone.
    QCOMPARE(transport.commits.size(), 1);
    const auto first = transport.commits.constFirst();
    QCOMPARE(first.operations.size(), 1);
    QCOMPARE(operationKey(first), QLatin1String(AccessibilityKeys::HighContrast));
    QCOMPARE(operationValue(first), QVariant(true));
    QCOMPARE(first.revision, quint64(7));

    QVariantMap afterFirst = defaultValues();
    afterFirst[QLatin1String(AccessibilityKeys::HighContrast)] = true;
    Q_EMIT transport.commitReceived(
        first.token, first.owner,
        commitWire(SettingsWireStatus::Applied, 7, 8,
                   {{QLatin1String(AccessibilityKeys::HighContrast), true}},
                   QStringLiteral("epoch-a")));
    // The next key is written only from the fresh post-commit snapshot.
    QCOMPARE(transport.commits.size(), 1);
    QVERIFY(model.saving());
    QVERIFY(answerSnapshot(transport, QStringLiteral("epoch-a"), 8, afterFirst));
    QTRY_COMPARE(transport.commits.size(), 2);
    const auto second = transport.commits.constLast();
    QCOMPARE(operationKey(second), QLatin1String(AccessibilityKeys::TextScale));
    QCOMPARE(operationValue(second).toDouble(), 1.5);
    QCOMPARE(second.revision, quint64(8));
    QVERIFY(model.saving());

    QVariantMap afterSecond = afterFirst;
    afterSecond[QLatin1String(AccessibilityKeys::TextScale)] = 1.5;
    Q_EMIT transport.commitReceived(
        second.token, second.owner,
        commitWire(SettingsWireStatus::Applied, 8, 9,
                   {{QLatin1String(AccessibilityKeys::TextScale), 1.5}},
                   QStringLiteral("epoch-a")));
    QVERIFY(answerSnapshot(transport, QStringLiteral("epoch-a"), 9, afterSecond));
    QTRY_VERIFY(model.ready());
    QVERIFY(!model.draftDirty());
    QVERIFY(model.highContrast());
    QCOMPARE(model.textScale(), 1.5);
    QVERIFY(model.canEdit());
    QVERIFY(model.errorText().isEmpty());
    QCOMPARE(transport.commits.size(), 2);
}

void AccessibilitySettingsModelTest::malformedRefreshFailsClosed()
{
    FakeSettingsTransport transport;
    SettingsClient client(transport, AccessibilityKeys::scopedKeys(), fastTiming());
    AccessibilitySettingsModel model(client);
    QVERIFY(client.start());
    QVERIFY(establishBaseline(transport, QStringLiteral(":1.33"),
                              QStringLiteral("epoch-a"), 2, defaultValues()));
    QTRY_VERIFY(model.ready());
    QVERIFY(model.setDraftReducedTransparency(true));

    QVariantMap malformed = defaultValues();
    malformed[QLatin1String(AccessibilityKeys::TextScale)] = 9.0;
    Q_EMIT transport.settingsChanged(QStringLiteral(":1.33"), QStringLiteral("epoch-a"),
                                     3, AccessibilityKeys::scopedKeys());
    QVERIFY(answerSnapshot(transport, QStringLiteral("epoch-a"), 3, malformed));
    QTRY_VERIFY(model.unavailable());
    QVERIFY(!model.canEdit());
    QVERIFY(!model.applyAvailable());
    QVERIFY(!model.applyDraft());
    QVERIFY(model.errorText().contains(QLatin1String(AccessibilityKeys::TextScale)));
    QCOMPARE(transport.commits.size(), 0);

    QVariantMap notBoolean = defaultValues();
    notBoolean[QLatin1String(AccessibilityKeys::HighContrast)] = QStringLiteral("yes");
    Q_EMIT transport.settingsChanged(QStringLiteral(":1.33"), QStringLiteral("epoch-a"),
                                     4, AccessibilityKeys::scopedKeys());
    QVERIFY(answerSnapshot(transport, QStringLiteral("epoch-a"), 4, notBoolean));
    QTRY_VERIFY(model.errorText().contains(QLatin1String(AccessibilityKeys::HighContrast)));
    QVERIFY(model.unavailable());
}

void AccessibilitySettingsModelTest::conflictStopsSequenceAndRequiresExplicitReapply()
{
    FakeSettingsTransport transport;
    SettingsClient client(transport, AccessibilityKeys::scopedKeys(), fastTiming());
    AccessibilitySettingsModel model(client);
    QVERIFY(client.start());
    QVERIFY(establishBaseline(transport, QStringLiteral(":1.34"),
                              QStringLiteral("epoch-a"), 5, defaultValues()));
    QTRY_VERIFY(model.ready());
    QVERIFY(model.setDraftReducedMotion(true));
    QVERIFY(model.setDraftTextScale(2.0));
    QVERIFY(model.applyDraft());
    QCOMPARE(transport.commits.size(), 1);

    // Another writer disabled reduced motion after our baseline.
    const auto first = transport.commits.constFirst();
    QVariantMap elsewhere = defaultValues();
    elsewhere[QLatin1String(AccessibilityKeys::TextScale)] = 1.25;
    Q_EMIT transport.commitReceived(
        first.token, first.owner,
        commitWire(SettingsWireStatus::Conflict, 6, 6,
                   {{QLatin1String(AccessibilityKeys::ReducedMotion), false}},
                   QStringLiteral("epoch-a"), QStringLiteral("changed elsewhere")));
    QVERIFY(answerSnapshot(transport, QStringLiteral("epoch-a"), 6, elsewhere));
    QTRY_VERIFY(model.conflict());
    // The sequence stopped: the text-scale key was never written.
    QCOMPARE(transport.commits.size(), 1);
    QVERIFY(model.draftDirty());
    QVERIFY(model.draftReducedMotion());
    QCOMPARE(model.draftTextScale(), 2.0);
    QCOMPARE(model.textScale(), 1.25);
    QVERIFY(model.canEdit());
    QVERIFY(model.applyAvailable());

    // Explicit re-Apply restates the whole remaining draft from fresh authority.
    QVERIFY(model.applyDraft());
    QVERIFY(model.saving());
    QCOMPARE(transport.commits.size(), 2);
    QCOMPARE(operationKey(transport.commits.constLast()),
             QLatin1String(AccessibilityKeys::ReducedMotion));
    QCOMPARE(transport.commits.constLast().revision, quint64(6));
}

void AccessibilitySettingsModelTest::uncertainWriteIsNeverReplayed()
{
    FakeSettingsTransport transport;
    SettingsClient client(transport, AccessibilityKeys::scopedKeys(), fastTiming(50));
    AccessibilitySettingsModel model(client);
    QVERIFY(client.start());
    QVERIFY(establishBaseline(transport, QStringLiteral(":1.35"),
                              QStringLiteral("epoch-a"), 2, defaultValues()));
    QTRY_VERIFY(model.ready());
    QVERIFY(model.setDraftHighContrast(true));
    QVERIFY(model.applyDraft());
    const auto commit = transport.commits.constLast();
    Q_EMIT transport.requestFailed(commit.token, commit.owner,
                                   QStringLiteral("timeout"),
                                   QStringLiteral("commit outcome uncertain"));
    QTRY_VERIFY(model.unavailable());
    QVERIFY(model.draftDirty());
    QVERIFY(model.draftHighContrast());
    QVERIFY(!model.canEdit());
    QTest::qWait(80);
    QCOMPARE(transport.commits.size(), 1);
    QVERIFY(model.errorText().contains(QStringLiteral("not replayed")));
}

void AccessibilitySettingsModelTest::replacementDuringSequenceAbortsWithoutReplay()
{
    FakeSettingsTransport transport;
    SettingsClient client(transport, AccessibilityKeys::scopedKeys(), fastTiming());
    AccessibilitySettingsModel model(client);
    QVERIFY(client.start());
    QVERIFY(establishBaseline(transport, QStringLiteral(":1.36"),
                              QStringLiteral("epoch-a"), 9, defaultValues()));
    QTRY_VERIFY(model.ready());
    QVERIFY(model.setDraftHighContrast(true));
    QVERIFY(model.setDraftReducedTransparency(true));
    QVERIFY(model.applyDraft());
    QCOMPARE(transport.commits.size(), 1);

    // Replacement authority arrives while the first key is still in flight.
    QVERIFY(establishBaseline(transport, QStringLiteral(":1.37"),
                              QStringLiteral("epoch-b"), 1, defaultValues()));
    QTRY_VERIFY(model.ready());
    QVERIFY(model.canEdit());
    QVERIFY(model.draftDirty());
    QVERIFY(model.draftHighContrast());
    QVERIFY(model.draftReducedTransparency());
    QVERIFY(!model.highContrast());
    QTest::qWait(40);
    QCOMPARE(transport.commits.size(), 1);
    QVERIFY(model.errorText().contains(QStringLiteral("not replayed")));
}

QTEST_GUILESS_MAIN(AccessibilitySettingsModelTest)
#include "tst_accessibility_settings_model.moc"
