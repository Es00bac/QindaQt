// SPDX-License-Identifier: GPL-3.0-or-later
#include "clipboard_settings_test_support.h"

#include <qindaqt/apps/settings_clipboard/clipboard_settings_model.h>
#include <qindaqt/services/clipboard_client/clipboard_client.h>
#include <qindaqt/services/settings_client/settings_client.h>

#include <QtTest>

using namespace QindaQt::Apps::SettingsClipboard;
using namespace QindaQt::Apps::SettingsClipboard::TestSupport;
namespace Services = QindaQt::Services;

namespace {
void establishSettings(FakeSettingsTransport &transport, const QString &owner,
                       const QString &epoch, quint64 revision, bool enabled)
{
    Q_EMIT transport.ownerChanged(owner);
    QTRY_VERIFY(!transport.snapshots.isEmpty());
    const auto request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(
        request.token, request.owner,
        settingsSnapshotWire(epoch, revision, enabled));
}
}

class ClipboardSettingsPreferenceTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void draftApplyConvergesFromOffDefault();
    void conflictRequiresExplicitChoice();
    void uncertainWriteIsNeverReplayed();
    void replacementPreservesDraftWithoutReplay();
};

void ClipboardSettingsPreferenceTest::draftApplyConvergesFromOffDefault()
{
    FakeSettingsTransport settingsTransport;
    FakeClipboardTransport clipboardTransport;
    Services::SettingsClient::SettingsClient settings(
        settingsTransport, {QString::fromLatin1(ClipboardHistorySettingsKey)},
        {.requestTimeoutMilliseconds = 100, .debounceMilliseconds = 0,
         .retryMilliseconds = {10}});
    Services::Clipboard::ClipboardClient clipboard(&clipboardTransport);
    ClipboardSettingsModel model(settings, clipboard);
    QVERIFY(settings.start());
    establishSettings(settingsTransport, QStringLiteral(":1.20"),
                      QStringLiteral("settings-a"), 4, false);

    QTRY_VERIFY(model.preferenceReady());
    QVERIFY(!model.historyEnabled());
    QVERIFY(!model.draftHistoryEnabled());
    QVERIFY(model.setDraftHistoryEnabled(true));
    QVERIFY(model.preferenceDirty());
    QVERIFY(model.applyPreference());
    QCOMPARE(settingsTransport.commits.size(), 1);
    const auto commit = settingsTransport.commits.constFirst();
    Q_EMIT settingsTransport.commitReceived(
        commit.token, commit.owner,
        settingsCommitWire(SettingsWireStatus::Applied, 4, 5, true,
                           QStringLiteral("settings-a")));
    QVERIFY(model.preferenceSaving());
    QTRY_VERIFY(!settingsTransport.snapshots.isEmpty());
    const auto refresh = settingsTransport.snapshots.takeFirst();
    Q_EMIT settingsTransport.snapshotReceived(
        refresh.token, refresh.owner,
        settingsSnapshotWire(QStringLiteral("settings-a"), 5, true));
    QTRY_VERIFY(model.preferenceReady());
    QVERIFY(model.historyEnabled());
    QVERIFY(!model.preferenceDirty());
    QCOMPARE(settingsTransport.commits.size(), 1);
}

void ClipboardSettingsPreferenceTest::conflictRequiresExplicitChoice()
{
    FakeSettingsTransport settingsTransport;
    FakeClipboardTransport clipboardTransport;
    Services::SettingsClient::SettingsClient settings(
        settingsTransport, {QString::fromLatin1(ClipboardHistorySettingsKey)},
        {.requestTimeoutMilliseconds = 100, .debounceMilliseconds = 0,
         .retryMilliseconds = {10}});
    Services::Clipboard::ClipboardClient clipboard(&clipboardTransport);
    ClipboardSettingsModel model(settings, clipboard);
    QVERIFY(settings.start());
    establishSettings(settingsTransport, QStringLiteral(":1.21"),
                      QStringLiteral("settings-a"), 7, false);
    QVERIFY(model.setDraftHistoryEnabled(true));
    QVERIFY(model.applyPreference());
    const auto commit = settingsTransport.commits.constLast();
    Q_EMIT settingsTransport.commitReceived(
        commit.token, commit.owner,
        settingsCommitWire(SettingsWireStatus::Conflict, 8, 8, false,
                           QStringLiteral("settings-a"),
                           QStringLiteral("changed elsewhere")));
    QTRY_VERIFY(!settingsTransport.snapshots.isEmpty());
    const auto refresh = settingsTransport.snapshots.takeFirst();
    Q_EMIT settingsTransport.snapshotReceived(
        refresh.token, refresh.owner,
        settingsSnapshotWire(QStringLiteral("settings-a"), 8, false));
    QTRY_VERIFY(model.preferenceConflict());
    QCOMPARE(settingsTransport.commits.size(), 1);
    QVERIFY(model.applyMyChoice());
    QCOMPARE(settingsTransport.commits.size(), 2);
}

void ClipboardSettingsPreferenceTest::uncertainWriteIsNeverReplayed()
{
    FakeSettingsTransport settingsTransport;
    FakeClipboardTransport clipboardTransport;
    Services::SettingsClient::SettingsClient settings(
        settingsTransport, {QString::fromLatin1(ClipboardHistorySettingsKey)},
        {.requestTimeoutMilliseconds = 50, .debounceMilliseconds = 0,
         .retryMilliseconds = {10}});
    Services::Clipboard::ClipboardClient clipboard(&clipboardTransport);
    ClipboardSettingsModel model(settings, clipboard);
    QVERIFY(settings.start());
    establishSettings(settingsTransport, QStringLiteral(":1.22"),
                      QStringLiteral("settings-a"), 2, false);
    QVERIFY(model.setDraftHistoryEnabled(true));
    QVERIFY(model.applyPreference());
    const auto commit = settingsTransport.commits.constLast();
    Q_EMIT settingsTransport.requestFailed(
        commit.token, commit.owner, QStringLiteral("timeout"),
        QStringLiteral("commit outcome uncertain"));
    QTRY_VERIFY(model.preferenceUnavailable());
    QVERIFY(model.preferenceDirty());
    QTest::qWait(80);
    QCOMPARE(settingsTransport.commits.size(), 1);
    QVERIFY(model.preferenceErrorText().contains(QStringLiteral("uncertain")));
}

void ClipboardSettingsPreferenceTest::replacementPreservesDraftWithoutReplay()
{
    FakeSettingsTransport settingsTransport;
    FakeClipboardTransport clipboardTransport;
    Services::SettingsClient::SettingsClient settings(
        settingsTransport, {QString::fromLatin1(ClipboardHistorySettingsKey)},
        {.requestTimeoutMilliseconds = 100, .debounceMilliseconds = 0,
         .retryMilliseconds = {10}});
    Services::Clipboard::ClipboardClient clipboard(&clipboardTransport);
    ClipboardSettingsModel model(settings, clipboard);
    QVERIFY(settings.start());
    establishSettings(settingsTransport, QStringLiteral(":1.23"),
                      QStringLiteral("settings-a"), 9, false);
    QVERIFY(model.setDraftHistoryEnabled(true));
    QVERIFY(model.applyPreference());
    QCOMPARE(settingsTransport.commits.size(), 1);
    establishSettings(settingsTransport, QStringLiteral(":1.24"),
                      QStringLiteral("settings-b"), 1, false);
    QTRY_VERIFY(model.preferenceReady());
    QVERIFY(model.preferenceDirty());
    QVERIFY(model.draftHistoryEnabled());
    QCOMPARE(settingsTransport.commits.size(), 1);
    QVERIFY(model.preferenceErrorText().contains(QStringLiteral("not replayed")));
}

QTEST_GUILESS_MAIN(ClipboardSettingsPreferenceTest)
#include "tst_clipboard_settings_preference.moc"
