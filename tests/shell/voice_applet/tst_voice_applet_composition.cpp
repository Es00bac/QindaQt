// SPDX-License-Identifier: GPL-3.0-or-later

#include "voiceappletcomposition.h"

#include <qindaqt/applet_host/capability_policy_loader.h>
#include <qindaqt/applets/manifest_catalog.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <qindaqt/services/voice_client/voice_transport.h>
#include <qindaqt/shell/voice_applet/voice_applet_controller.h>

#include <QtTest>

using namespace QindaQt;
using Services::SettingsProtocol::SettingsWireStatus;
using Services::SettingsProtocol::WireContract;

namespace {

class FakeSettingsTransport final : public Services::SettingsClient::SettingsTransport {
    Q_OBJECT
public:
    struct Read { quint64 token; QString owner; };
    QList<Read> reads;

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
    void commit(quint64, const QString &, const QString &, quint64,
                const QVariantList &) override {}
    void requestActivation() override {}

    void answer(quint64 revision, bool input, bool transcript)
    {
        const Read read = reads.takeFirst();
        const QVariantMap values{
            {QStringLiteral("services.voiceInput"), input},
            {QStringLiteral("services.voicePanelTranscript"), transcript}};
        const QVariantMap sources{
            {QStringLiteral("services.voiceInput"), QStringLiteral("user-overrides")},
            {QStringLiteral("services.voicePanelTranscript"),
             QStringLiteral("user-overrides")}};
        const QVariantMap wire{
            {QLatin1StringView(WireContract::FieldStatus),
             quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch),
             QStringLiteral("voice-epoch")},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), sources},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
        Q_EMIT snapshotReceived(read.token, read.owner, wire);
    }
};

class ActivationCountingVoiceTransport final : public Services::Voice::VoiceTransport {
    Q_OBJECT
public:
    int starts = 0;
    int stops = 0;

    void start() override { ++starts; }
    void stop() override { ++stops; }
    void fetchSnapshot(const QString &, quint64) override {}
    void submitOperation(const QString &, quint64,
                         const Services::Voice::OperationRequest &) override {}
};

} // namespace

class VoiceAppletCompositionTests final : public QObject {
    Q_OBJECT
private slots:
    void defaultOffAndOwnerReplacementFenceActivation();
};

void VoiceAppletCompositionTests::defaultOffAndOwnerReplacementFenceActivation()
{
    Applets::ManifestCatalog catalog;
    QString error;
    QVERIFY2(catalog.loadDirectory(
                 QStringLiteral(QINDAQT_SOURCE_DIR "/data/applets"), &error),
             qPrintable(error));
    const auto loaded = AppletHost::CapabilityPolicyLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/applet-policy/default.json"));
    QVERIFY2(loaded.ok, qPrintable(loaded.error));

    FakeSettingsTransport settingsTransport;
    Services::SettingsClient::SettingsClient settings(
        settingsTransport,
        {QStringLiteral("services.voiceInput"),
         QStringLiteral("services.voicePanelTranscript")},
        {.requestTimeoutMilliseconds = 300,
         .debounceMilliseconds = 0,
         .retryMilliseconds = {10}});
    ActivationCountingVoiceTransport voiceTransport;
    Shell::VoiceAppletComposition composition(
        catalog, loaded.policy, settings, voiceTransport);
    auto *applet = composition.access();
    QVERIFY(applet != nullptr);
    QCOMPARE(voiceTransport.starts, 0);
    applet->setExpanded(true);
    QCOMPARE(voiceTransport.starts, 0); // popup refresh cannot bypass Off

    QVERIFY(settings.start());
    Q_EMIT settingsTransport.ownerChanged(QStringLiteral(":1.10"));
    QTRY_COMPARE(settingsTransport.reads.size(), 1);
    QCOMPARE(voiceTransport.starts, 0); // owner alone is not consent
    settingsTransport.answer(1, false, true);
    QCOMPARE(voiceTransport.starts, 0);
    applet->setExpanded(false);
    applet->setExpanded(true);
    QCOMPARE(voiceTransport.starts, 0);

    settings.refresh();
    QTRY_COMPARE(settingsTransport.reads.size(), 1);
    settingsTransport.answer(2, true, true);
    QCOMPARE(voiceTransport.starts, 1);
    settings.refresh();
    QTRY_COMPARE(settingsTransport.reads.size(), 1);
    settingsTransport.answer(3, true, false);
    QCOMPARE(voiceTransport.starts, 1); // transcript preference is independent

    Q_EMIT settingsTransport.ownerChanged(QStringLiteral(":1.11"));
    QCOMPARE(voiceTransport.stops, 1);
    applet->setExpanded(false);
    applet->setExpanded(true);
    QCOMPARE(voiceTransport.starts, 1);
    QTRY_COMPARE(settingsTransport.reads.size(), 1);
    settingsTransport.answer(1, false, true);
    QCOMPARE(voiceTransport.starts, 1);
    settings.refresh();
    QTRY_COMPARE(settingsTransport.reads.size(), 1);
    settingsTransport.answer(2, true, true);
    QCOMPARE(voiceTransport.starts, 2);
    Q_EMIT settingsTransport.ownerChanged(QString{});
    QCOMPARE(voiceTransport.stops, 2);
    QCOMPARE(voiceTransport.starts, 2);
}

QTEST_GUILESS_MAIN(VoiceAppletCompositionTests)
#include "tst_voice_applet_composition.moc"
