// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_voice/voice_settings_model.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <qindaqt/services/voice_client/voice_client.h>
#include <qindaqt/services/voice_client/voice_transport.h>
#include <qindaqt/services/voice_preferences/voice_input_preference_gate.h>

#include <QSignalSpy>
#include <QtTest>

#ifdef QINDAQT_HAVE_VOICE_CONSOLE
#include "voice_console_model.h"
#endif

using namespace QindaQt::Apps::SettingsVoice;
using namespace QindaQt::Services;
using SettingsProtocol::SettingsWireStatus;
using SettingsProtocol::WireContract;

namespace {
const QString inputKey = QStringLiteral("services.voiceInput");
const QString transcriptKey = QStringLiteral("services.voicePanelTranscript");
const QString epoch = QStringLiteral("voice-epoch");

class FakeSettingsTransport final : public SettingsClient::SettingsTransport {
    Q_OBJECT
public:
    struct Read { quint64 token; QString owner; };
    struct Write { quint64 token; QString owner; quint64 revision; QString key; };
    QList<Read> reads;
    QList<Write> writes;

    bool start(QString *error) override
    {
        if (error) error->clear();
        return true;
    }
    void stop() override {}
    void requestSnapshot(quint64 token, const QString &owner,
                         const QStringList &) override
    {
        reads.append({token, owner});
    }
    void commit(quint64 token, const QString &owner, const QString &,
                quint64 revision, const QVariantList &operations) override
    {
        writes.append({token, owner, revision,
                       operations.constFirst().toMap()
                           .value(QLatin1StringView(WireContract::FieldKey))
                           .toString()});
    }
    void requestActivation() override {}

    void answer(quint64 revision, bool input, bool transcript)
    {
        const Read read = reads.takeFirst();
        const QVariantMap values{{inputKey, input}, {transcriptKey, transcript}};
        const QVariantMap sources{
            {inputKey, QStringLiteral("user-overrides")},
            {transcriptKey, QStringLiteral("user-overrides")}};
        const QVariantMap wire{
            {QLatin1StringView(WireContract::FieldStatus),
             quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), epoch},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), sources},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
        Q_EMIT snapshotReceived(read.token, read.owner, wire);
    }

    void answerCommit(SettingsWireStatus status, quint64 after, bool value)
    {
        const Write write = writes.constLast();
        const QVariantMap values{{write.key, value}};
        const QVariantMap sources{{write.key, QStringLiteral("user-overrides")}};
        const QVariantMap wire{
            {QLatin1StringView(WireContract::FieldStatus), quint32(status)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), epoch},
            {QLatin1StringView(WireContract::FieldRevisionBefore),
             status == SettingsWireStatus::Applied ? write.revision : after},
            {QLatin1StringView(WireContract::FieldRevisionAfter), after},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), sources},
            {QLatin1StringView(WireContract::FieldChangedKeys),
             status == SettingsWireStatus::Applied
                 ? QStringList{write.key} : QStringList{}},
            {QLatin1StringView(WireContract::FieldMessage),
             status == SettingsWireStatus::Applied ? QString{}
                                                    : QStringLiteral("conflict")}};
        Q_EMIT commitReceived(write.token, write.owner, wire);
    }
};

class FakeVoiceTransport final : public Voice::VoiceTransport {
    Q_OBJECT
public:
    int starts = 0;
    int stops = 0;
    void start() override { ++starts; }
    void stop() override { ++stops; }
    void fetchSnapshot(const QString &, quint64) override {}
    void submitOperation(const QString &, quint64,
                         const Voice::OperationRequest &) override {}
};

SettingsClient::ClientTiming timing()
{
    return {.requestTimeoutMilliseconds = 300,
            .debounceMilliseconds = 0,
            .retryMilliseconds = {10}};
}

void startAndAnswer(FakeSettingsTransport &transport,
                    SettingsClient::SettingsClient &client,
                    bool input, bool transcript)
{
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.10"));
    QTRY_COMPARE(transport.reads.size(), 1);
    transport.answer(1, input, transcript);
    QCOMPARE(client.state(), SettingsClient::ClientState::Ready);
}

} // namespace

class VoiceSettingsTests final : public QObject {
    Q_OBJECT
private slots:
    void gateRequiresConfirmedCurrentOwner();
    void sameOwnerTranscriptCommitDoesNotInterruptVoice();
    void cleanDraftFollowsExternalSnapshot();
    void offRouteCanSaveWhileProviderStopped();
    void sequentialApplyWaitsForFreshRevision();
    void conflictStopsSecondKeyWithoutReplay();
    void conflictThenExternalOffKeepsRouteWithdrawn();
    void conflictThenExternalOnReopensAfterReadback();
    void uncertainOffWaitsForConfirmedReadback();
    void uncertainFirstWritePreservesDraftWithoutReplay();
    void ownerReplacementDuringWriteRevokesDraftWithoutReplay();
#ifdef QINDAQT_HAVE_VOICE_CONSOLE
    void consoleRetryRespectsDesktopGate();
#endif
};

void VoiceSettingsTests::gateRequiresConfirmedCurrentOwner()
{
    FakeSettingsTransport transport;
    SettingsClient::SettingsClient settings(transport, {inputKey, transcriptKey},
                                             timing());
    VoicePreferences::VoiceInputPreferenceGate gate(settings);
    FakeVoiceTransport voiceTransport;
    Voice::VoiceClient voice(&voiceTransport);
    QObject::connect(&gate, &VoicePreferences::VoiceInputPreferenceGate::allowedChanged,
                     &voice, [&voice](bool allowed) {
                         if (allowed) voice.start();
                         else voice.stop();
                     });
    QVERIFY(!gate.allowed());
    QVERIFY(settings.start());
    QCOMPARE(voiceTransport.starts, 0);
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.10"));
    QTRY_COMPARE(transport.reads.size(), 1);
    QCOMPARE(voiceTransport.starts, 0);
    transport.answer(1, false, true);
    QVERIFY(!gate.allowed());
    QCOMPARE(voiceTransport.starts, 0);

    settings.refresh();
    QTRY_COMPARE(transport.reads.size(), 1);
    transport.answer(2, true, true);
    QVERIFY(gate.allowed());
    QCOMPARE(voiceTransport.starts, 1);

    Q_EMIT transport.ownerChanged(QStringLiteral(":1.11"));
    QVERIFY(!gate.allowed());
    QCOMPARE(voice.state(), Voice::ClientState::Stopped);
    QCOMPARE(voiceTransport.starts, 1);
    QTRY_COMPARE(transport.reads.size(), 1);
    transport.answer(1, false, true);
    QVERIFY(!gate.allowed());
    QCOMPARE(voiceTransport.starts, 1);
    Q_EMIT transport.busDisconnected();
    QVERIFY(!gate.allowed());
}

void VoiceSettingsTests::sameOwnerTranscriptCommitDoesNotInterruptVoice()
{
    FakeSettingsTransport transport;
    SettingsClient::SettingsClient settings(transport, {inputKey, transcriptKey},
                                             timing());
    VoicePreferences::VoiceInputPreferenceGate gate(settings);
    startAndAnswer(transport, settings, true, true);
    QVERIFY(gate.allowed());
    QSignalSpy changes(&gate,
                       &VoicePreferences::VoiceInputPreferenceGate::allowedChanged);
    QVERIFY(settings.setUserValue(transcriptKey, false));
    QCOMPARE(transport.writes.size(), 1);
    transport.answerCommit(SettingsWireStatus::Applied, 2, false);
    QCOMPARE(settings.state(), SettingsClient::ClientState::Authenticating);
    QVERIFY(gate.allowed());
    QCOMPARE(changes.size(), 0);
    QTRY_COMPARE(transport.reads.size(), 1);
    transport.answer(2, true, false);
    QVERIFY(gate.allowed());
    QCOMPARE(changes.size(), 0);
}

void VoiceSettingsTests::cleanDraftFollowsExternalSnapshot()
{
    FakeSettingsTransport transport;
    SettingsClient::SettingsClient settings(transport, {inputKey, transcriptKey},
                                             timing());
    FakeVoiceTransport voiceTransport;
    Voice::VoiceClient voice(&voiceTransport);
    VoiceSettingsModel model(settings, voice);
    startAndAnswer(transport, settings, false, true);
    QVERIFY(!model.preferenceDirty());
    settings.refresh();
    QTRY_COMPARE(transport.reads.size(), 1);
    transport.answer(2, true, false);
    QVERIFY(model.voiceInputEnabled());
    QVERIFY(model.draftVoiceInputEnabled());
    QVERIFY(!model.panelTranscriptEnabled());
    QVERIFY(!model.draftPanelTranscriptEnabled());
    QVERIFY(!model.preferenceDirty());
}

void VoiceSettingsTests::offRouteCanSaveWhileProviderStopped()
{
    FakeSettingsTransport transport;
    SettingsClient::SettingsClient settings(transport, {inputKey, transcriptKey},
                                             timing());
    VoicePreferences::VoiceInputPreferenceGate gate(settings);
    FakeVoiceTransport voiceTransport;
    Voice::VoiceClient voice(&voiceTransport);
    VoiceSettingsModel model(settings, voice);
    QObject::connect(&gate, &VoicePreferences::VoiceInputPreferenceGate::allowedChanged,
                     &model, [&voice](bool allowed) {
                         if (allowed) voice.start();
                         else voice.stop();
                     });
    startAndAnswer(transport, settings, false, true);
    QCOMPARE(voice.state(), Voice::ClientState::Stopped);
    QVERIFY(model.canEditPreference());
    QVERIFY(model.setDraftVoiceInputEnabled(true));
    QVERIFY(model.applyPreferences());
    QCOMPARE(voiceTransport.starts, 0);
    QCOMPARE(transport.writes.size(), 1);
    transport.answerCommit(SettingsWireStatus::Applied, 2, true);
    QCOMPARE(voiceTransport.starts, 0);
    QTRY_COMPARE(transport.reads.size(), 1);
    transport.answer(2, true, true);
    QCOMPARE(voiceTransport.starts, 1);
    QVERIFY(model.preferenceReady());
}

void VoiceSettingsTests::sequentialApplyWaitsForFreshRevision()
{
    FakeSettingsTransport transport;
    SettingsClient::SettingsClient settings(transport, {inputKey, transcriptKey},
                                             timing());
    FakeVoiceTransport voiceTransport;
    Voice::VoiceClient voice(&voiceTransport);
    VoiceSettingsModel model(settings, voice);
    startAndAnswer(transport, settings, false, true);
    QVERIFY(model.setDraftVoiceInputEnabled(true));
    QVERIFY(model.setDraftPanelTranscriptEnabled(false));
    QVERIFY(model.applyPreferences());
    QCOMPARE(transport.writes.size(), 1);
    QCOMPARE(transport.writes.constLast().key, inputKey);
    QCOMPARE(transport.writes.constLast().revision, quint64(1));
    transport.answerCommit(SettingsWireStatus::Applied, 2, true);
    QCOMPARE(transport.writes.size(), 1);
    QVERIFY(model.preferenceSaving());
    QTRY_COMPARE(transport.reads.size(), 1);
    transport.answer(2, true, true);
    QCOMPARE(transport.writes.size(), 2);
    QCOMPARE(transport.writes.constLast().key, transcriptKey);
    QCOMPARE(transport.writes.constLast().revision, quint64(2));
    transport.answerCommit(SettingsWireStatus::Applied, 3, false);
    QTRY_COMPARE(transport.reads.size(), 1);
    transport.answer(3, true, false);
    QVERIFY(model.preferenceReady());
    QVERIFY(!model.preferenceDirty());
}

void VoiceSettingsTests::conflictStopsSecondKeyWithoutReplay()
{
    FakeSettingsTransport transport;
    SettingsClient::SettingsClient settings(transport, {inputKey, transcriptKey},
                                             timing());
    FakeVoiceTransport voiceTransport;
    Voice::VoiceClient voice(&voiceTransport);
    VoiceSettingsModel model(settings, voice);
    startAndAnswer(transport, settings, false, true);
    QVERIFY(model.setDraftVoiceInputEnabled(true));
    QVERIFY(model.setDraftPanelTranscriptEnabled(false));
    QVERIFY(model.applyPreferences());
    transport.answerCommit(SettingsWireStatus::Applied, 2, true);
    QTRY_COMPARE(transport.reads.size(), 1);
    transport.answer(2, true, true);
    QCOMPARE(transport.writes.size(), 2);
    transport.answerCommit(SettingsWireStatus::Conflict, 3, true);
    QVERIFY(model.preferenceLoading());
    QVERIFY(!model.canEditPreference());
    QVERIFY(!model.preferenceErrorText().isEmpty());
    QCOMPARE(transport.writes.size(), 2);
    QTRY_COMPARE(transport.reads.size(), 1);
    transport.answer(3, true, true);
    QVERIFY(model.preferenceDirty());
    QCOMPARE(transport.writes.size(), 2);
}


void VoiceSettingsTests::conflictThenExternalOffKeepsRouteWithdrawn()
{
    FakeSettingsTransport transport;
    SettingsClient::SettingsClient settings(transport, {inputKey, transcriptKey},
                                             timing());
    VoicePreferences::VoiceInputPreferenceGate gate(settings);
    FakeVoiceTransport voiceTransport;
    Voice::VoiceClient voice(&voiceTransport);
    VoiceSettingsModel model(settings, voice);
    auto sync = [&] {
        if (gate.allowed() && !model.voiceUseWithdrawn()) voice.start();
        else voice.stop();
    };
    QObject::connect(&gate, &VoicePreferences::VoiceInputPreferenceGate::allowedChanged,
                     &model, sync);
    QObject::connect(&model, &VoiceSettingsModel::viewChanged, &model, sync);
    startAndAnswer(transport, settings, true, true);
    QCOMPARE(voiceTransport.starts, 1);
    QVERIFY(model.setDraftVoiceInputEnabled(false));
    QVERIFY(model.applyPreferences());
    QVERIFY(model.voiceUseWithdrawn());
    QCOMPARE(voice.state(), Voice::ClientState::Stopped);
    QCOMPARE(transport.writes.size(), 1);
    // Another writer committed Off at revision 2. The conflicting reply is
    // not authority for the route, and the old On gate must not restart it.
    transport.answerCommit(SettingsWireStatus::Conflict, 2, false);
    QVERIFY(model.preferenceLoading());
    QVERIFY(model.voiceUseWithdrawn());
    QCOMPARE(voiceTransport.starts, 1);
    QTRY_COMPARE(transport.reads.size(), 1);
    // A stale same-owner read cannot release the local Off withdrawal.
    transport.answer(1, true, true);
    QVERIFY(model.voiceUseWithdrawn());
    QCOMPARE(voiceTransport.starts, 1);
    QTRY_COMPARE(transport.reads.size(), 1);
    transport.answer(2, false, true);
    QVERIFY(model.preferenceReady());
    QVERIFY(!model.voiceUseWithdrawn());
    QVERIFY(!gate.allowed());
    QCOMPARE(voiceTransport.starts, 1);
    QCOMPARE(transport.writes.size(), 1);
}

void VoiceSettingsTests::conflictThenExternalOnReopensAfterReadback()
{
    FakeSettingsTransport transport;
    SettingsClient::SettingsClient settings(transport, {inputKey, transcriptKey},
                                             timing());
    VoicePreferences::VoiceInputPreferenceGate gate(settings);
    FakeVoiceTransport voiceTransport;
    Voice::VoiceClient voice(&voiceTransport);
    VoiceSettingsModel model(settings, voice);
    auto sync = [&] {
        if (gate.allowed() && !model.voiceUseWithdrawn()) voice.start();
        else voice.stop();
    };
    QObject::connect(&gate, &VoicePreferences::VoiceInputPreferenceGate::allowedChanged,
                     &model, sync);
    QObject::connect(&model, &VoiceSettingsModel::viewChanged, &model, sync);
    startAndAnswer(transport, settings, true, true);
    QCOMPARE(voiceTransport.starts, 1);
    QVERIFY(model.setDraftVoiceInputEnabled(false));
    QVERIFY(model.applyPreferences());
    QVERIFY(model.voiceUseWithdrawn());
    QCOMPARE(voice.state(), Voice::ClientState::Stopped);
    // Another writer advanced the revision but kept Voice On.
    transport.answerCommit(SettingsWireStatus::Conflict, 2, true);
    QVERIFY(model.voiceUseWithdrawn());
    QCOMPARE(voiceTransport.starts, 1);
    QTRY_COMPARE(transport.reads.size(), 1);
    transport.answer(2, true, true);
    QVERIFY(model.preferenceReady());
    QVERIFY(!model.voiceUseWithdrawn());
    QVERIFY(gate.allowed());
    QCOMPARE(voiceTransport.starts, 2);
    QVERIFY(model.preferenceDirty());
    QCOMPARE(transport.writes.size(), 1);
}

void VoiceSettingsTests::uncertainOffWaitsForConfirmedReadback()
{
    FakeSettingsTransport transport;
    SettingsClient::SettingsClient settings(transport, {inputKey, transcriptKey},
                                             timing());
    VoicePreferences::VoiceInputPreferenceGate gate(settings);
    FakeVoiceTransport voiceTransport;
    Voice::VoiceClient voice(&voiceTransport);
    VoiceSettingsModel model(settings, voice);
    auto sync = [&] {
        if (gate.allowed() && !model.voiceUseWithdrawn()) voice.start();
        else voice.stop();
    };
    QObject::connect(&gate, &VoicePreferences::VoiceInputPreferenceGate::allowedChanged,
                     &model, sync);
    QObject::connect(&model, &VoiceSettingsModel::viewChanged, &model, sync);
    startAndAnswer(transport, settings, true, true);
    QCOMPARE(voiceTransport.starts, 1);
    QVERIFY(model.setDraftVoiceInputEnabled(false));
    QVERIFY(model.applyPreferences());
    QCOMPARE(voice.state(), Voice::ClientState::Stopped);
    const auto write = transport.writes.constLast();
    Q_EMIT transport.requestFailed(write.token, write.owner,
                                   QStringLiteral("transport-error"),
                                   QStringLiteral("lost reply"));
    QVERIFY(model.voiceUseWithdrawn());
    QCOMPARE(voiceTransport.starts, 1);
    QTRY_COMPARE(transport.reads.size(), 1);
    // The old confirmed On must not leak through Ready before snapshotChanged.
    transport.answer(2, true, true);
    QVERIFY(!model.voiceUseWithdrawn());
    QCOMPARE(voiceTransport.starts, 2);
    QCOMPARE(transport.writes.size(), 1);
    QVERIFY(model.preferenceDirty());
}

void VoiceSettingsTests::uncertainFirstWritePreservesDraftWithoutReplay()
{
    FakeSettingsTransport transport;
    SettingsClient::SettingsClient settings(transport, {inputKey, transcriptKey},
                                             timing());
    FakeVoiceTransport voiceTransport;
    Voice::VoiceClient voice(&voiceTransport);
    VoiceSettingsModel model(settings, voice);
    startAndAnswer(transport, settings, false, true);
    QVERIFY(model.setDraftVoiceInputEnabled(true));
    QVERIFY(model.setDraftPanelTranscriptEnabled(false));
    QVERIFY(model.applyPreferences());
    QCOMPARE(transport.writes.size(), 1);
    const auto write = transport.writes.constLast();
    Q_EMIT transport.requestFailed(write.token, write.owner,
                                   QStringLiteral("transport-error"),
                                   QStringLiteral("lost reply"));
    QVERIFY(model.preferenceUnavailable());
    QCOMPARE(transport.writes.size(), 1);
    QTRY_COMPARE(transport.reads.size(), 1);
    // The first write might have landed. Readback resolves its value, while
    // the still-unattempted transcript request remains a user draft.
    transport.answer(2, true, true);
    QVERIFY(model.preferenceReady());
    QVERIFY(model.draftVoiceInputEnabled());
    QVERIFY(!model.draftPanelTranscriptEnabled());
    QVERIFY(model.preferenceDirty());
    QCOMPARE(transport.writes.size(), 1);
}

void VoiceSettingsTests::ownerReplacementDuringWriteRevokesDraftWithoutReplay()
{
    FakeSettingsTransport transport;
    SettingsClient::SettingsClient settings(transport, {inputKey, transcriptKey},
                                             timing());
    VoicePreferences::VoiceInputPreferenceGate gate(settings);
    FakeVoiceTransport voiceTransport;
    Voice::VoiceClient voice(&voiceTransport);
    VoiceSettingsModel model(settings, voice);
    startAndAnswer(transport, settings, true, true);
    QVERIFY(gate.allowed());
    QVERIFY(model.setDraftVoiceInputEnabled(false));
    QVERIFY(model.applyPreferences());
    QCOMPARE(transport.writes.size(), 1);
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.11"));
    QVERIFY(!gate.allowed());
    QCOMPARE(transport.writes.size(), 1);
    QTRY_COMPARE(transport.reads.size(), 1);
    transport.answer(1, true, true);
    QVERIFY(gate.allowed());
    QVERIFY(model.preferenceReady());
    QVERIFY(!model.preferenceDirty());
    QCOMPARE(transport.writes.size(), 1);
}

#ifdef QINDAQT_HAVE_VOICE_CONSOLE
void VoiceSettingsTests::consoleRetryRespectsDesktopGate()
{
    FakeSettingsTransport transport;
    SettingsClient::SettingsClient settings(transport, {inputKey, transcriptKey},
                                             timing());
    VoicePreferences::VoiceInputPreferenceGate gate(settings);
    FakeVoiceTransport voiceTransport;
    Voice::VoiceClient voice(&voiceTransport);
    QindaQt::Apps::Voice::VoiceConsoleModel console(voice);
    QObject::connect(&gate, &VoicePreferences::VoiceInputPreferenceGate::allowedChanged,
                     &console, [&voice](bool allowed) {
                         if (allowed) voice.start();
                         else voice.stop();
                     });
    QObject::connect(&console,
                     &QindaQt::Apps::Voice::VoiceConsoleModel::connectionRetryRequested,
                     &console, [&voice, &gate] {
                         if (gate.allowed()) {
                             voice.stop();
                             voice.start();
                         }
                     });
    QSignalSpy retries(&console,
                       &QindaQt::Apps::Voice::VoiceConsoleModel::connectionRetryRequested);
    startAndAnswer(transport, settings, false, true);
    QVERIFY(!console.canRetryConnection());
    console.retryConnection();
    QCOMPARE(retries.size(), 0);
    QCOMPARE(voiceTransport.starts, 0);

    settings.refresh();
    QTRY_COMPARE(transport.reads.size(), 1);
    transport.answer(2, true, true);
    QCOMPARE(voiceTransport.starts, 1);
    Q_EMIT voiceTransport.ownerChanged(QString{});
    QVERIFY(console.canRetryConnection());
    console.retryConnection();
    QCOMPARE(retries.size(), 1);
    QCOMPARE(voiceTransport.starts, 2);

    settings.refresh();
    QTRY_COMPARE(transport.reads.size(), 1);
    transport.answer(3, false, true);
    QVERIFY(!console.canRetryConnection());
    console.retryConnection();
    QCOMPARE(retries.size(), 1);
    QCOMPARE(voiceTransport.starts, 2);
}
#endif

QTEST_GUILESS_MAIN(VoiceSettingsTests)
#include "tst_voice_settings.moc"
