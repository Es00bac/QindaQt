// SPDX-License-Identifier: GPL-3.0-or-later

// The org.qindaqt.Voice1 wire contract: what a provider is allowed to say.
//
// AGENT-CONTRACT: the "real provider payload" rows below carry values captured
// verbatim from Gabbee's src/gabbee/qindaqt_voice.py over a private session
// bus. They are the cross-implementation evidence that that encoder and this
// decoder agree; regenerate them from a live capture, never by hand.

#include <qindaqt/services/voice_protocol/voice_dbus.h>
#include <qindaqt/services/voice_protocol/voice_validation.h>

#include <QtTest/QtTest>

using namespace QindaQt::Services::Voice;

namespace {

// Captured from Gabbee on a private bus, 2026-09-21. Key order is irrelevant;
// the key set and the value types are the contract.
QVariantMap capturedGabbeeSnapshot()
{
    return {
        {QStringLiteral("capabilities"), quint32(63)},
        {QStringLiteral("commandShortcut"), QStringLiteral("F6")},
        {QStringLiteral("dictationShortcut"), QStringLiteral("F5")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("languageCode"), QStringLiteral("en")},
        {QStringLiteral("lastRoute"), quint32(1)},
        {QStringLiteral("lastText"), QStringLiteral("the quick brown fox")},
        {QStringLiteral("microphoneLabel"), QStringLiteral("alsa_input.usb-Blue_Yeti")},
        {QStringLiteral("mode"), quint32(0)},
        {QStringLiteral("partialText"), QStringLiteral("the quick brown")},
        {QStringLiteral("providerAvailableMask"), quint32(1)},
        {QStringLiteral("providerId"), QStringLiteral("elevenlabs")},
        {QStringLiteral("providerIds"),
         QStringList{QStringLiteral("elevenlabs"), QStringLiteral("gemini"),
                     QStringLiteral("whisper_local")}},
        {QStringLiteral("providerLabel"), QStringLiteral("ElevenLabs Scribe")},
        {QStringLiteral("providerLabels"),
         QStringList{QStringLiteral("ElevenLabs Scribe"), QStringLiteral("Google Gemini"),
                     QStringLiteral("Whisper (on this computer)")}},
        {QStringLiteral("reasonCode"), QStringLiteral("ok")},
        {QStringLiteral("revision"), quint64(1)},
        {QStringLiteral("schemaVersion"), quint32(1)},
        {QStringLiteral("state"), quint32(3)},
    };
}

QVariantMap capturedGabbeeRefusal()
{
    return {
        {QStringLiteral("initiatingRevision"), quint64(1)},
        {QStringLiteral("kind"), quint32(0)},
        {QStringLiteral("observedRevision"), quint64(1)},
        {QStringLiteral("reasonCode"), QStringLiteral("already-capturing")},
        {QStringLiteral("requestId"), quint64(42)},
        {QStringLiteral("status"), quint32(1)},
    };
}

Snapshot validSnapshot()
{
    Snapshot snapshot;
    snapshot.revision = 4;
    snapshot.state = SessionState::Idle;
    snapshot.mode = CaptureMode::Dictation;
    snapshot.enabled = true;
    snapshot.capabilities = kKnownCapabilities;
    snapshot.lastRoute = DeliveryRoute::InputMethod;
    snapshot.providerId = QStringLiteral("elevenlabs");
    snapshot.providerLabel = QStringLiteral("ElevenLabs Scribe");
    snapshot.languageCode = QStringLiteral("en");
    snapshot.microphoneLabel = QStringLiteral("Yeti");
    snapshot.dictationShortcut = QStringLiteral("F5");
    snapshot.commandShortcut = QStringLiteral("F6");
    snapshot.lastText = QStringLiteral("hello");
    snapshot.reasonCode = QStringLiteral("ok");
    snapshot.providers = {ProviderDescriptor{.id = QStringLiteral("elevenlabs"),
                                             .label = QStringLiteral("ElevenLabs"),
                                             .available = true}};
    return snapshot;
}

} // namespace

class VoiceProtocolTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void decodesARealGabbeeSnapshot();
    void decodesARealGabbeeRefusal();
    void snapshotRoundTrips();
    void resultRoundTrips();
    void rejectsAnUnknownKey();
    void rejectsAMissingKey();
    void rejectsAWronglyTypedKey();
    void rejectsAStringWhereANumberBelongs();
    void rejectsAnUnknownSessionState();
    void rejectsAnUnknownCapabilityBit();
    void rejectsAZeroRevision();
    void rejectsAStalePartial();
    void rejectsAnOversizedTranscript();
    void rejectsTerminalEscapesInATranscript();
    void acceptsSpokenLayoutInATranscript();
    void rejectsAnUnstructuredReasonCode();
    void rejectsACurrentProviderMissingFromTheList();
    void rejectsAnAvailabilityBitPastTheList();
    void rejectsTooManyProviders();
    void decodesAProviderListSentAsVariants();
    void refusesAProviderListElementThatIsNotAString();
    void refusesAnOversizedProviderArray();
    void rejectsARegressedResultRevision();
    void rejectsACommandModeTheProviderCannotDo();
    void clampsTheLevel();
    void capabilityFloorIsAlwaysOffered();
    void argumentsCarryOnlyWhatTheKindNeeds();
};

void VoiceProtocolTest::decodesARealGabbeeSnapshot()
{
    const Snapshot snapshot = decodeSnapshot(capturedGabbeeSnapshot());
    QVERIFY(snapshot.wireValid);
    const ValidationResult validation = validateSnapshot(snapshot);
    QVERIFY2(validation.accepted, qPrintable(validation.reasonCode));
    QCOMPARE(snapshot.state, SessionState::Listening);
    QCOMPARE(snapshot.lastRoute, DeliveryRoute::InputMethod);
    QCOMPARE(snapshot.providerId, QStringLiteral("elevenlabs"));
    QCOMPARE(snapshot.partialText, QStringLiteral("the quick brown"));
    QCOMPARE(snapshot.providers.size(), 3);
    QCOMPARE(snapshot.providers.at(0).available, true);
    QCOMPARE(snapshot.providers.at(1).available, false);
    QCOMPARE(snapshot.providers.at(2).id, QStringLiteral("whisper_local"));
}

void VoiceProtocolTest::decodesARealGabbeeRefusal()
{
    const OperationResult result = decodeOperationResult(capturedGabbeeRefusal());
    QVERIFY(result.wireValid);
    QVERIFY(validateOperationResult(result).accepted);
    QCOMPARE(result.kind, OperationKind::StartDictation);
    QCOMPARE(result.status, OperationStatus::Rejected);
    QCOMPARE(result.requestId, quint64(42));
    QCOMPARE(result.reasonCode, QStringLiteral("already-capturing"));
}

void VoiceProtocolTest::snapshotRoundTrips()
{
    const Snapshot original = validSnapshot();
    const Snapshot decoded = decodeSnapshot(encodeSnapshot(original));
    QVERIFY(decoded.wireValid);
    QCOMPARE(decoded, original);
}

void VoiceProtocolTest::resultRoundTrips()
{
    const OperationResult original{.kind = OperationKind::CopyLast,
                                   .status = OperationStatus::Uncertain,
                                   .requestId = 12,
                                   .initiatingRevision = 3,
                                   .observedRevision = 5,
                                   .reasonCode = QStringLiteral("operation-timeout")};
    const OperationResult decoded =
        decodeOperationResult(encodeOperationResult(original));
    QVERIFY(decoded.wireValid);
    QCOMPARE(decoded, original);
}

void VoiceProtocolTest::rejectsAnUnknownKey()
{
    QVariantMap payload = capturedGabbeeSnapshot();
    payload.insert(QStringLiteral("somethingNewer"), 1);
    QVERIFY(!decodeSnapshot(payload).wireValid);
}

void VoiceProtocolTest::rejectsAMissingKey()
{
    QVariantMap payload = capturedGabbeeSnapshot();
    payload.remove(QStringLiteral("languageCode"));
    QVERIFY(!decodeSnapshot(payload).wireValid);
}

void VoiceProtocolTest::rejectsAWronglyTypedKey()
{
    QVariantMap payload = capturedGabbeeSnapshot();
    payload.insert(QStringLiteral("enabled"), quint32(1));
    QVERIFY(!decodeSnapshot(payload).wireValid);
}

void VoiceProtocolTest::rejectsAStringWhereANumberBelongs()
{
    QVariantMap payload = capturedGabbeeSnapshot();
    payload.insert(QStringLiteral("revision"), QStringLiteral("7"));
    QVERIFY(!decodeSnapshot(payload).wireValid);
}

void VoiceProtocolTest::rejectsAnUnknownSessionState()
{
    Snapshot snapshot = validSnapshot();
    snapshot.state = static_cast<SessionState>(99);
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("unknown-state"));
    snapshot.state = SessionState::Unknown;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("unknown-state"));
}

void VoiceProtocolTest::rejectsAnUnknownCapabilityBit()
{
    Snapshot snapshot = validSnapshot();
    snapshot.capabilities |= 1u << 20;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("unknown-capability"));
}

void VoiceProtocolTest::rejectsAZeroRevision()
{
    Snapshot snapshot = validSnapshot();
    snapshot.revision = 0;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-revision"));
}

void VoiceProtocolTest::rejectsAStalePartial()
{
    Snapshot snapshot = validSnapshot();
    snapshot.state = SessionState::Idle;
    snapshot.partialText = QStringLiteral("half a sentence");
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("stale-partial"));
    snapshot.state = SessionState::Listening;
    QVERIFY(validateSnapshot(snapshot).accepted);
}

void VoiceProtocolTest::rejectsAnOversizedTranscript()
{
    Snapshot snapshot = validSnapshot();
    snapshot.lastText = QString(kMaxTranscriptUtf8Bytes + 1, QLatin1Char('a'));
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("malformed-transcript"));
}

void VoiceProtocolTest::rejectsTerminalEscapesInATranscript()
{
    Snapshot snapshot = validSnapshot();
    snapshot.lastText = QStringLiteral("red") + QChar(0x1B) + QStringLiteral("[31m");
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("malformed-transcript"));
}

void VoiceProtocolTest::acceptsSpokenLayoutInATranscript()
{
    Snapshot snapshot = validSnapshot();
    // "new paragraph" and "tab" are things a person dictates.
    snapshot.lastText = QStringLiteral("first\n\nsecond\tthird");
    QVERIFY(validateSnapshot(snapshot).accepted);
}

void VoiceProtocolTest::rejectsAnUnstructuredReasonCode()
{
    Snapshot snapshot = validSnapshot();
    snapshot.reasonCode = QStringLiteral("Could not reach the server!");
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("malformed-reason"));
    snapshot.reasonCode.clear();
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("malformed-reason"));
}

void VoiceProtocolTest::rejectsACurrentProviderMissingFromTheList()
{
    Snapshot snapshot = validSnapshot();
    snapshot.providers = {ProviderDescriptor{.id = QStringLiteral("gemini"),
                                             .label = QStringLiteral("Gemini"),
                                             .available = true}};
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("provider-not-listed"));
}

void VoiceProtocolTest::rejectsAnAvailabilityBitPastTheList()
{
    QVariantMap payload = capturedGabbeeSnapshot();
    payload.insert(QStringLiteral("providerAvailableMask"), quint32(1u << 5));
    QVERIFY(!decodeSnapshot(payload).wireValid);
}

void VoiceProtocolTest::rejectsTooManyProviders()
{
    Snapshot snapshot = validSnapshot();
    for (int index = 0; index <= kMaxProviders; ++index) {
        snapshot.providers.append(
            ProviderDescriptor{.id = QStringLiteral("p%1").arg(index),
                               .label = QStringLiteral("P"),
                               .available = true});
    }
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("too-many-providers"));
}

void VoiceProtocolTest::decodesAProviderListSentAsVariants()
{
    // PyQt6 marshals a Python list of str as `av`, not `as`, so the provider
    // inventory reaches us as variants. Gabbee really does send this shape;
    // it was found by the live interop probe, not by inspection.
    QVariantMap payload = capturedGabbeeSnapshot();
    payload.insert(QStringLiteral("providerIds"),
                   QVariantList{QStringLiteral("elevenlabs"), QStringLiteral("gemini"),
                                QStringLiteral("whisper_local")});
    payload.insert(QStringLiteral("providerLabels"),
                   QVariantList{QStringLiteral("ElevenLabs Scribe"),
                                QStringLiteral("Google Gemini"),
                                QStringLiteral("Whisper (on this computer)")});
    const Snapshot snapshot = decodeSnapshot(payload);
    QVERIFY(snapshot.wireValid);
    QCOMPARE(snapshot.providers.size(), 3);
    QCOMPARE(snapshot.providers.at(2).id, QStringLiteral("whisper_local"));
    QCOMPARE(snapshot.providers.at(0).label, QStringLiteral("ElevenLabs Scribe"));
}

void VoiceProtocolTest::refusesAProviderListElementThatIsNotAString()
{
    QVariantMap payload = capturedGabbeeSnapshot();
    payload.insert(QStringLiteral("providerIds"),
                   QVariantList{QStringLiteral("elevenlabs"), quint32(7),
                                QStringLiteral("whisper_local")});
    QVERIFY(!decodeSnapshot(payload).wireValid);
}

void VoiceProtocolTest::refusesAnOversizedProviderArray()
{
    // AGENT-NOTE: defence in depth. The bound lives in the decoder so an
    // oversized array is refused before anything is built from it, rather
    // than relying on the caller to notice afterwards. What actually caused
    // the 26GB allocation this fix came from -- a mistyped array off a real
    // bus spinning a hand-written demarshalling loop -- cannot be built in a
    // unit test; session.voice-interop is what covers that.
    QStringList ids;
    QStringList labels;
    for (int index = 0; index <= kMaxProviders; ++index) {
        ids.append(QStringLiteral("p%1").arg(index));
        labels.append(QStringLiteral("P"));
    }
    QVariantMap payload = capturedGabbeeSnapshot();
    payload.insert(QStringLiteral("providerIds"), ids);
    payload.insert(QStringLiteral("providerLabels"), labels);
    QVERIFY(!decodeSnapshot(payload).wireValid);

    QVariantList variantIds;
    for (const QString &id : ids) {
        variantIds.append(id);
    }
    payload.insert(QStringLiteral("providerIds"), variantIds);
    QVERIFY(!decodeSnapshot(payload).wireValid);
}

void VoiceProtocolTest::rejectsARegressedResultRevision()
{
    const OperationResult result{.kind = OperationKind::Finish,
                                 .status = OperationStatus::Succeeded,
                                 .requestId = 1,
                                 .initiatingRevision = 9,
                                 .observedRevision = 8,
                                 .reasonCode = QStringLiteral("ok")};
    QCOMPARE(validateOperationResult(result).reasonCode,
             QStringLiteral("revision-regressed"));
}

void VoiceProtocolTest::rejectsACommandModeTheProviderCannotDo()
{
    Snapshot snapshot = validSnapshot();
    snapshot.mode = CaptureMode::Command;
    snapshot.capabilities = kKnownCapabilities & ~quint32(CapabilityCommandMode);
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("unsupported-mode"));
}

void VoiceProtocolTest::clampsTheLevel()
{
    QCOMPARE(clampLevelPercent(0), 0u);
    QCOMPARE(clampLevelPercent(100), 100u);
    QCOMPARE(clampLevelPercent(4'000'000'000u), 100u);
}

void VoiceProtocolTest::capabilityFloorIsAlwaysOffered()
{
    // Starting, finishing, cancelling and arming are the contract floor: a
    // provider that answers Voice1 at all can do them.
    QVERIFY(capabilityForKind(OperationKind::StartDictation, CapabilityNone));
    QVERIFY(capabilityForKind(OperationKind::Finish, CapabilityNone));
    QVERIFY(capabilityForKind(OperationKind::Cancel, CapabilityNone));
    QVERIFY(capabilityForKind(OperationKind::SetEnabled, CapabilityNone));
    QVERIFY(!capabilityForKind(OperationKind::Undo, CapabilityNone));
    QVERIFY(capabilityForKind(OperationKind::Undo, CapabilityUndo));
}

void VoiceProtocolTest::argumentsCarryOnlyWhatTheKindNeeds()
{
    OperationRequest request;
    request.kind = OperationKind::Finish;
    request.requestId = 3;
    request.expectedRevision = 7;
    QCOMPARE(argumentsForRequest(request).size(), 2);
    QCOMPARE(methodNameForKind(request.kind), QStringLiteral("Finish"));

    request.kind = OperationKind::SetProvider;
    request.providerId = QStringLiteral("gemini");
    const QVariantList provider = argumentsForRequest(request);
    QCOMPARE(provider.size(), 3);
    QCOMPARE(provider.at(2).toString(), QStringLiteral("gemini"));

    request.kind = OperationKind::SetEnabled;
    request.providerId.clear();
    request.enable = true;
    const QVariantList enabled = argumentsForRequest(request);
    QCOMPARE(enabled.size(), 3);
    QCOMPARE(enabled.at(2).toBool(), true);
}

QTEST_MAIN(VoiceProtocolTest)
#include "tst_voice_protocol.moc"
