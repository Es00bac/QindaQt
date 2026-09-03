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
void publishClipboard(FakeClipboardTransport &transport, const QString &owner,
                      const Services::Clipboard::Snapshot &snapshot)
{
    Q_EMIT transport.ownerChanged(owner);
    Q_EMIT transport.snapshotReply(owner, transport.fetchToken, true, snapshot,
                                   QString{});
}

Services::Clipboard::OperationResult clearResult(
    const Services::Clipboard::OperationRequest &request,
    Services::Clipboard::OperationStatus status, quint64 observedRevision,
    QString reason = QStringLiteral("ok"))
{
    return {.kind = Services::Clipboard::OperationKind::Clear,
            .status = status,
            .requestId = request.requestId,
            .initiatingEpoch = request.expectedEpoch,
            .initiatingGeneration = request.expectedGeneration,
            .initiatingRevision = request.expectedRevision,
            .observedEpoch = request.expectedEpoch,
            .observedGeneration = request.expectedGeneration,
            .observedRevision = observedRevision,
            .reasonCode = std::move(reason)};
}
}

class ClipboardSettingsServiceTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void projectsOnlyCountCapacityAndPrivacyState();
    void confirmationAndClearUseExactLineage();
    void changedSnapshotInvalidatesConfirmation();
    void uncertainClearIsNotReplayed();
};

void ClipboardSettingsServiceTest::projectsOnlyCountCapacityAndPrivacyState()
{
    FakeSettingsTransport settingsTransport;
    FakeClipboardTransport clipboardTransport;
    Services::SettingsClient::SettingsClient settings(
        settingsTransport, {QString::fromLatin1(ClipboardHistorySettingsKey)});
    Services::Clipboard::ClipboardClient clipboard(&clipboardTransport);
    ClipboardSettingsModel model(settings, clipboard);
    clipboard.start();
    publishClipboard(clipboardTransport, QStringLiteral(":1.40"),
                     clipboardSnapshot(12, 3, 8, 3));
    QTRY_VERIFY(model.serviceAvailable());
    QCOMPARE(model.serviceState(), QStringLiteral("available"));
    QCOMPARE(model.entryCount(), 3);
    QCOMPARE(model.capacity(), Services::ClipboardModel::kMaxEntries);
    QCOMPARE(model.serviceEpoch(), qulonglong(12));
    QCOMPARE(model.serviceGeneration(), qulonglong(3));
    QCOMPARE(model.serviceRevision(), qulonglong(8));
    QVERIFY(model.clearAvailable());

    Q_EMIT clipboardTransport.invalidated(QStringLiteral(":1.40"), 12, 4, 0);
    Q_EMIT clipboardTransport.snapshotReply(
        QStringLiteral(":1.40"), clipboardTransport.fetchToken, true,
        clipboardSnapshot(12, 4, 0, 0, true, false), QString{});
    QTRY_VERIFY(model.privacyDenied());
    QCOMPARE(model.serviceState(), QStringLiteral("privacy-denied"));
    QCOMPARE(model.entryCount(), 0);
    QVERIFY(!model.clearAvailable());

    Q_EMIT clipboardTransport.invalidated(QStringLiteral(":1.40"), 12, 5, 0);
    Q_EMIT clipboardTransport.snapshotReply(
        QStringLiteral(":1.40"), clipboardTransport.fetchToken, true,
        clipboardSnapshot(12, 5, 0, 0, false, false), QString{});
    QTRY_VERIFY(!model.privacyDenied());
    QCOMPARE(model.serviceState(), QStringLiteral("available"));
    QVERIFY(model.serviceStatusText().contains(QStringLiteral("off")));
}

void ClipboardSettingsServiceTest::confirmationAndClearUseExactLineage()
{
    FakeSettingsTransport settingsTransport;
    FakeClipboardTransport clipboardTransport;
    Services::SettingsClient::SettingsClient settings(
        settingsTransport, {QString::fromLatin1(ClipboardHistorySettingsKey)});
    Services::Clipboard::ClipboardClient clipboard(&clipboardTransport);
    ClipboardSettingsModel model(settings, clipboard);
    clipboard.start();
    publishClipboard(clipboardTransport, QStringLiteral(":1.41"),
                     clipboardSnapshot(14, 6, 20, 2));
    QVERIFY(model.requestClearHistory());
    QVERIFY(model.clearConfirmationPending());
    QVERIFY(model.confirmClearHistory());
    QCOMPARE(clipboardTransport.operations.size(), 1);
    const auto submitted = clipboardTransport.operations.constFirst();
    QCOMPARE(submitted.owner, QStringLiteral(":1.41"));
    QCOMPARE(submitted.request.kind, Services::Clipboard::OperationKind::Clear);
    QVERIFY(submitted.request.clearAll);
    QCOMPARE(submitted.request.expectedEpoch, quint64(14));
    QCOMPARE(submitted.request.expectedGeneration, quint32(6));
    QCOMPARE(submitted.request.expectedRevision, quint64(20));
    QVERIFY(model.clearBusy());

    Q_EMIT clipboardTransport.operationReply(
        submitted.owner, submitted.token, true,
        clearResult(submitted.request,
                    Services::Clipboard::OperationStatus::Succeeded, 21),
        QString{});
    QTRY_VERIFY(model.clearBusy());
    Q_EMIT clipboardTransport.snapshotReply(
        submitted.owner, clipboardTransport.fetchToken, true,
        clipboardSnapshot(14, 6, 21, 0), QString{});
    QTRY_VERIFY(!model.clearBusy());
    QCOMPARE(model.entryCount(), 0);
    QVERIFY(model.clearStatusText().contains(QStringLiteral("cleared")));
}

void ClipboardSettingsServiceTest::changedSnapshotInvalidatesConfirmation()
{
    FakeSettingsTransport settingsTransport;
    FakeClipboardTransport clipboardTransport;
    Services::SettingsClient::SettingsClient settings(
        settingsTransport, {QString::fromLatin1(ClipboardHistorySettingsKey)});
    Services::Clipboard::ClipboardClient clipboard(&clipboardTransport);
    ClipboardSettingsModel model(settings, clipboard);
    clipboard.start();
    publishClipboard(clipboardTransport, QStringLiteral(":1.42"),
                     clipboardSnapshot(15, 2, 5, 1));
    QVERIFY(model.requestClearHistory());
    Q_EMIT clipboardTransport.invalidated(QStringLiteral(":1.42"), 15, 2, 6);
    Q_EMIT clipboardTransport.snapshotReply(
        QStringLiteral(":1.42"), clipboardTransport.fetchToken, true,
        clipboardSnapshot(15, 2, 6, 2), QString{});
    QTRY_VERIFY(!model.clearConfirmationPending());
    QVERIFY(!model.confirmClearHistory());
    QCOMPARE(clipboardTransport.operations.size(), 0);
}

void ClipboardSettingsServiceTest::uncertainClearIsNotReplayed()
{
    FakeSettingsTransport settingsTransport;
    FakeClipboardTransport clipboardTransport;
    Services::SettingsClient::SettingsClient settings(
        settingsTransport, {QString::fromLatin1(ClipboardHistorySettingsKey)});
    Services::Clipboard::ClipboardClient clipboard(&clipboardTransport);
    ClipboardSettingsModel model(settings, clipboard);
    clipboard.start();
    publishClipboard(clipboardTransport, QStringLiteral(":1.43"),
                     clipboardSnapshot(16, 4, 9, 1));
    QVERIFY(model.requestClearHistory());
    QVERIFY(model.confirmClearHistory());
    const auto submitted = clipboardTransport.operations.constFirst();
    // AGENT-NOTE: Regression for Fern Hunt P1.3. Pending-to-Uncertain changes
    // Q_PROPERTY values, so it must notify the QML page immediately.
    QSignalSpy viewSpy(&model, &ClipboardSettingsModel::viewChanged);
    Q_EMIT clipboardTransport.operationReply(
        submitted.owner, submitted.token, false, {},
        QStringLiteral("transport-timeout"));
    QTRY_VERIFY(model.clearUncertain());
    QTRY_VERIFY(viewSpy.count() > 0);
    QTest::qWait(30);
    QCOMPARE(clipboardTransport.operations.size(), 1);
    QVERIFY(model.clearStatusText().contains(QStringLiteral("not retried")));
}

QTEST_GUILESS_MAIN(ClipboardSettingsServiceTest)
#include "tst_clipboard_settings_service.moc"
