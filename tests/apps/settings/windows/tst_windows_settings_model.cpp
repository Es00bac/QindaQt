// SPDX-License-Identifier: GPL-3.0-or-later
#include "windows_settings_test_support.h"

#include <qindaqt/apps/settings_windows/windows_settings_model.h>
#include <qindaqt/services/settings_client/settings_client.h>

#include <QtTest>

using namespace QindaQt::Apps::SettingsWindows;
using namespace QindaQt::Apps::SettingsWindows::TestSupport;
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

class WindowsSettingsModelTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void loadingThenReadyScopesExactlyFourKeys();
    void draftSettersGateTokensAndRangeAndRevertRestores();
    void applySequencesPerKeyCommitsInOrder();
    void malformedRefreshFailsClosed();
    void conflictStopsSequenceAndRequiresExplicitReapply();
    void uncertainWriteIsNeverReplayed();
    void replacementDuringSequenceAbortsWithoutReplay();
    void choiceListsCoverEverySchemaToken();
};

void WindowsSettingsModelTest::loadingThenReadyScopesExactlyFourKeys()
{
    FakeSettingsTransport transport;
    SettingsClient client(transport, WindowsKeys::scopedKeys(), fastTiming());
    WindowsSettingsModel model(client);
    QVERIFY(model.loading());
    QVERIFY(!model.canEdit());
    QVERIFY(!model.draftDirty());
    QVERIFY(client.start());

    QVariantMap values = defaultValues();
    values[QLatin1String(WindowsKeys::FocusPolicy)] = QStringLiteral("focus-follows-mouse");
    // A floating JSON number that is integral is a valid distance on the wire.
    values[QLatin1String(WindowsKeys::SnapDistance)] = 20.0;
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.30"));
    QTRY_VERIFY(!transport.snapshots.isEmpty());
    const auto request = transport.snapshots.constFirst();
    // AGENT-GUARD: windowManagement.sessionRestore has no consumer and must
    // never be scoped, read, or written by this route.
    QCOMPARE(request.keys.size(), 4);
    QVERIFY(request.keys.contains(QLatin1String(WindowsKeys::FocusPolicy)));
    QVERIFY(request.keys.contains(QLatin1String(WindowsKeys::DockingModifier)));
    QVERIFY(request.keys.contains(QLatin1String(WindowsKeys::SnapDistance)));
    QVERIFY(request.keys.contains(QLatin1String(WindowsKeys::CloseContainerPolicy)));
    for (const QString &key : request.keys) {
        QVERIFY(!key.contains(QStringLiteral("Restore")));
    }
    QVERIFY(answerSnapshot(transport, QStringLiteral("epoch-a"), 3, values));

    QTRY_VERIFY(model.ready());
    QVERIFY(model.canEdit());
    QVERIFY(!model.draftDirty());
    QVERIFY(!model.applyAvailable());
    QCOMPARE(model.focusPolicy(), QStringLiteral("focus-follows-mouse"));
    QCOMPARE(model.draftFocusPolicy(), QStringLiteral("focus-follows-mouse"));
    QCOMPARE(model.dockingModifier(), QStringLiteral("super"));
    QCOMPARE(model.snapDistance(), 20);
    QCOMPARE(model.draftSnapDistance(), 20);
    QCOMPARE(model.closeContainerPolicy(), QStringLiteral("ask"));
    QCOMPARE(model.minimumSnapDistance(), 0);
    QCOMPARE(model.maximumSnapDistance(), 64);
    QCOMPARE(model.defaultSnapDistance(), 12);
    QVERIFY(model.statusText().isEmpty());
    QVERIFY(model.errorText().isEmpty());
}

void WindowsSettingsModelTest::draftSettersGateTokensAndRangeAndRevertRestores()
{
    FakeSettingsTransport transport;
    SettingsClient client(transport, WindowsKeys::scopedKeys(), fastTiming());
    WindowsSettingsModel model(client);
    // No baseline: every setter is refused rather than silently buffered.
    QVERIFY(!model.setDraftFocusPolicy(QStringLiteral("click")));
    QVERIFY(!model.setDraftSnapDistance(8));
    QVERIFY(client.start());
    QVERIFY(establishBaseline(transport, QStringLiteral(":1.31"),
                              QStringLiteral("epoch-a"), 4, defaultValues()));
    QTRY_VERIFY(model.ready());

    // Tokens outside the schema are refused and never dirty the draft.
    QVERIFY(!model.setDraftFocusPolicy(QStringLiteral("sloppy")));
    QVERIFY(!model.setDraftDockingModifier(QStringLiteral("hyper")));
    QVERIFY(!model.setDraftCloseContainerPolicy(QStringLiteral("nuke")));
    QVERIFY(!model.draftDirty());
    QVERIFY(!model.setDraftSnapDistance(65));
    QVERIFY(!model.snapDistanceError().isEmpty());
    QVERIFY(!model.setDraftSnapDistance(-1));
    QVERIFY(!model.draftDirty());
    QVERIFY(model.setDraftSnapDistance(0));
    QVERIFY(model.snapDistanceError().isEmpty());
    QVERIFY(model.setDraftDockingModifier(QStringLiteral("disabled")));
    QVERIFY(model.setDraftCloseContainerPolicy(QStringLiteral("ungroup")));
    QVERIFY(model.draftDirty());
    QVERIFY(model.applyAvailable());
    QVERIFY(model.statusText().contains(QStringLiteral("not yet applied")));
    QCOMPARE(model.dockingModifier(), QStringLiteral("super"));
    QCOMPARE(model.snapDistance(), 12);

    QVERIFY(model.revertDraft());
    QVERIFY(!model.draftDirty());
    QCOMPARE(model.draftDockingModifier(), QStringLiteral("super"));
    QCOMPARE(model.draftSnapDistance(), 12);
    QCOMPARE(model.draftCloseContainerPolicy(), QStringLiteral("ask"));
    QVERIFY(!model.revertDraft());
}

void WindowsSettingsModelTest::applySequencesPerKeyCommitsInOrder()
{
    FakeSettingsTransport transport;
    SettingsClient client(transport, WindowsKeys::scopedKeys(), fastTiming());
    WindowsSettingsModel model(client);
    QVERIFY(client.start());
    QVERIFY(establishBaseline(transport, QStringLiteral(":1.32"),
                              QStringLiteral("epoch-a"), 7, defaultValues()));
    QTRY_VERIFY(model.ready());

    QVERIFY(model.setDraftSnapDistance(24));
    QVERIFY(model.setDraftDockingModifier(QStringLiteral("alt")));
    QVERIFY(model.applyDraft());
    QVERIFY(model.saving());
    QVERIFY(!model.canEdit());

    // The first changed key in scoped order commits alone.
    QCOMPARE(transport.commits.size(), 1);
    const auto first = transport.commits.constFirst();
    QCOMPARE(first.operations.size(), 1);
    QCOMPARE(operationKey(first), QLatin1String(WindowsKeys::DockingModifier));
    QCOMPARE(operationValue(first), QVariant(QStringLiteral("alt")));
    QCOMPARE(first.revision, quint64(7));

    QVariantMap afterFirst = defaultValues();
    afterFirst[QLatin1String(WindowsKeys::DockingModifier)] = QStringLiteral("alt");
    Q_EMIT transport.commitReceived(
        first.token, first.owner,
        commitWire(SettingsWireStatus::Applied, 7, 8,
                   {{QLatin1String(WindowsKeys::DockingModifier), QStringLiteral("alt")}},
                   QStringLiteral("epoch-a")));
    // The next key is written only from the fresh post-commit snapshot.
    QCOMPARE(transport.commits.size(), 1);
    QVERIFY(model.saving());
    QVERIFY(answerSnapshot(transport, QStringLiteral("epoch-a"), 8, afterFirst));
    QTRY_COMPARE(transport.commits.size(), 2);
    const auto second = transport.commits.constLast();
    QCOMPARE(operationKey(second), QLatin1String(WindowsKeys::SnapDistance));
    QCOMPARE(operationValue(second).toInt(), 24);
    QCOMPARE(second.revision, quint64(8));
    QVERIFY(model.saving());

    QVariantMap afterSecond = afterFirst;
    afterSecond[QLatin1String(WindowsKeys::SnapDistance)] = 24;
    Q_EMIT transport.commitReceived(
        second.token, second.owner,
        commitWire(SettingsWireStatus::Applied, 8, 9,
                   {{QLatin1String(WindowsKeys::SnapDistance), 24}},
                   QStringLiteral("epoch-a")));
    QVERIFY(answerSnapshot(transport, QStringLiteral("epoch-a"), 9, afterSecond));
    QTRY_VERIFY(model.ready());
    QVERIFY(!model.draftDirty());
    QCOMPARE(model.dockingModifier(), QStringLiteral("alt"));
    QCOMPARE(model.snapDistance(), 24);
    QVERIFY(model.canEdit());
    QVERIFY(model.errorText().isEmpty());
    QCOMPARE(transport.commits.size(), 2);
}

void WindowsSettingsModelTest::malformedRefreshFailsClosed()
{
    FakeSettingsTransport transport;
    SettingsClient client(transport, WindowsKeys::scopedKeys(), fastTiming());
    WindowsSettingsModel model(client);
    QVERIFY(client.start());
    QVERIFY(establishBaseline(transport, QStringLiteral(":1.33"),
                              QStringLiteral("epoch-a"), 2, defaultValues()));
    QTRY_VERIFY(model.ready());
    QVERIFY(model.setDraftCloseContainerPolicy(QStringLiteral("close-all")));

    QVariantMap malformed = defaultValues();
    malformed[QLatin1String(WindowsKeys::SnapDistance)] = 90;
    Q_EMIT transport.settingsChanged(QStringLiteral(":1.33"), QStringLiteral("epoch-a"),
                                     3, WindowsKeys::scopedKeys());
    QVERIFY(answerSnapshot(transport, QStringLiteral("epoch-a"), 3, malformed));
    QTRY_VERIFY(model.unavailable());
    QVERIFY(!model.canEdit());
    QVERIFY(!model.applyAvailable());
    QVERIFY(!model.applyDraft());
    QVERIFY(model.errorText().contains(QLatin1String(WindowsKeys::SnapDistance)));
    QCOMPARE(transport.commits.size(), 0);

    QVariantMap unknownToken = defaultValues();
    unknownToken[QLatin1String(WindowsKeys::FocusPolicy)] = QStringLiteral("sloppy");
    Q_EMIT transport.settingsChanged(QStringLiteral(":1.33"), QStringLiteral("epoch-a"),
                                     4, WindowsKeys::scopedKeys());
    QVERIFY(answerSnapshot(transport, QStringLiteral("epoch-a"), 4, unknownToken));
    QTRY_VERIFY(model.errorText().contains(QLatin1String(WindowsKeys::FocusPolicy)));
    QVERIFY(model.unavailable());
    // The draft survives the fail-closed refresh for an explicit re-Apply later.
    QCOMPARE(model.draftCloseContainerPolicy(), QStringLiteral("close-all"));
}

void WindowsSettingsModelTest::conflictStopsSequenceAndRequiresExplicitReapply()
{
    FakeSettingsTransport transport;
    SettingsClient client(transport, WindowsKeys::scopedKeys(), fastTiming());
    WindowsSettingsModel model(client);
    QVERIFY(client.start());
    QVERIFY(establishBaseline(transport, QStringLiteral(":1.34"),
                              QStringLiteral("epoch-a"), 5, defaultValues()));
    QTRY_VERIFY(model.ready());
    QVERIFY(model.setDraftFocusPolicy(QStringLiteral("focus-under-mouse")));
    QVERIFY(model.setDraftSnapDistance(32));
    QVERIFY(model.applyDraft());
    QCOMPARE(transport.commits.size(), 1);

    // Another writer changed the focus policy after our baseline.
    const auto first = transport.commits.constFirst();
    QVariantMap elsewhere = defaultValues();
    elsewhere[QLatin1String(WindowsKeys::FocusPolicy)] = QStringLiteral("focus-follows-mouse");
    Q_EMIT transport.commitReceived(
        first.token, first.owner,
        commitWire(SettingsWireStatus::Conflict, 6, 6,
                   {{QLatin1String(WindowsKeys::FocusPolicy),
                     QStringLiteral("focus-follows-mouse")}},
                   QStringLiteral("epoch-a"), QStringLiteral("changed elsewhere")));
    QVERIFY(answerSnapshot(transport, QStringLiteral("epoch-a"), 6, elsewhere));
    QTRY_VERIFY(model.conflict());
    // The sequence stopped: the snap distance was never written.
    QCOMPARE(transport.commits.size(), 1);
    QVERIFY(model.draftDirty());
    QCOMPARE(model.draftFocusPolicy(), QStringLiteral("focus-under-mouse"));
    QCOMPARE(model.draftSnapDistance(), 32);
    QCOMPARE(model.focusPolicy(), QStringLiteral("focus-follows-mouse"));
    QVERIFY(model.canEdit());
    QVERIFY(model.applyAvailable());

    // Explicit re-Apply restates the whole remaining draft from fresh authority.
    QVERIFY(model.applyDraft());
    QVERIFY(model.saving());
    QCOMPARE(transport.commits.size(), 2);
    QCOMPARE(operationKey(transport.commits.constLast()),
             QLatin1String(WindowsKeys::FocusPolicy));
    QCOMPARE(transport.commits.constLast().revision, quint64(6));
}

void WindowsSettingsModelTest::uncertainWriteIsNeverReplayed()
{
    FakeSettingsTransport transport;
    SettingsClient client(transport, WindowsKeys::scopedKeys(), fastTiming(50));
    WindowsSettingsModel model(client);
    QVERIFY(client.start());
    QVERIFY(establishBaseline(transport, QStringLiteral(":1.35"),
                              QStringLiteral("epoch-a"), 2, defaultValues()));
    QTRY_VERIFY(model.ready());
    QVERIFY(model.setDraftDockingModifier(QStringLiteral("control")));
    QVERIFY(model.applyDraft());
    const auto commit = transport.commits.constLast();
    Q_EMIT transport.requestFailed(commit.token, commit.owner,
                                   QStringLiteral("timeout"),
                                   QStringLiteral("commit outcome uncertain"));
    QTRY_VERIFY(model.unavailable());
    QVERIFY(model.draftDirty());
    QCOMPARE(model.draftDockingModifier(), QStringLiteral("control"));
    QVERIFY(!model.canEdit());
    QTest::qWait(80);
    QCOMPARE(transport.commits.size(), 1);
    QVERIFY(model.errorText().contains(QStringLiteral("not replayed")));
}

void WindowsSettingsModelTest::replacementDuringSequenceAbortsWithoutReplay()
{
    FakeSettingsTransport transport;
    SettingsClient client(transport, WindowsKeys::scopedKeys(), fastTiming());
    WindowsSettingsModel model(client);
    QVERIFY(client.start());
    QVERIFY(establishBaseline(transport, QStringLiteral(":1.36"),
                              QStringLiteral("epoch-a"), 9, defaultValues()));
    QTRY_VERIFY(model.ready());
    QVERIFY(model.setDraftFocusPolicy(QStringLiteral("focus-follows-mouse")));
    QVERIFY(model.setDraftCloseContainerPolicy(QStringLiteral("ungroup")));
    QVERIFY(model.applyDraft());
    QCOMPARE(transport.commits.size(), 1);

    // Replacement authority arrives while the first key is still in flight.
    QVERIFY(establishBaseline(transport, QStringLiteral(":1.37"),
                              QStringLiteral("epoch-b"), 1, defaultValues()));
    QTRY_VERIFY(model.ready());
    QVERIFY(model.canEdit());
    QVERIFY(model.draftDirty());
    QCOMPARE(model.draftFocusPolicy(), QStringLiteral("focus-follows-mouse"));
    QCOMPARE(model.draftCloseContainerPolicy(), QStringLiteral("ungroup"));
    QCOMPARE(model.focusPolicy(), QStringLiteral("click"));
    QTest::qWait(40);
    QCOMPARE(transport.commits.size(), 1);
    QVERIFY(model.errorText().contains(QStringLiteral("not replayed")));
}

void WindowsSettingsModelTest::choiceListsCoverEverySchemaToken()
{
    // AGENT-GUARD: QTest macros expand `return;`, so the lambda only
    // collects; the label check happens in the test body.
    const auto tokensOf = [](const QVariantList &choices) {
        QStringList tokens;
        for (const QVariant &choice : choices) {
            tokens.append(choice.toMap().value(QStringLiteral("token")).toString());
        }
        return tokens;
    };
    for (const QVariantList &choices : {WindowsSettingsModel::focusPolicyChoices(),
                                        WindowsSettingsModel::dockingModifierChoices(),
                                        WindowsSettingsModel::closeContainerPolicyChoices()}) {
        for (const QVariant &choice : choices) {
            QVERIFY(!choice.toMap().value(QStringLiteral("label")).toString().isEmpty());
        }
    }
    QCOMPARE(tokensOf(WindowsSettingsModel::focusPolicyChoices()),
             WindowsValues::focusPolicyTokens());
    QCOMPARE(tokensOf(WindowsSettingsModel::dockingModifierChoices()),
             WindowsValues::dockingModifierTokens());
    QCOMPARE(tokensOf(WindowsSettingsModel::closeContainerPolicyChoices()),
             WindowsValues::closeContainerPolicyTokens());
}

QTEST_GUILESS_MAIN(WindowsSettingsModelTest)
#include "tst_windows_settings_model.moc"
