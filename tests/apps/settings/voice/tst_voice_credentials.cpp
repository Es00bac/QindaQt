// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_voice/voice_credentials_model.h>
#include <qindaqt/services/voice_configuration/voice_configuration_client.h>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QtTest/QTest>
#include <memory>
Q_IMPORT_QML_PLUGIN(QindaQtSettingsVoicePlugin)
using namespace QindaQt::Services::VoiceConfiguration;
using QindaQt::Apps::SettingsVoice::VoiceCredentialsModel;
using namespace Qt::StringLiterals;
namespace {
QVariantMap snapshot() {
    return {{u"schemaVersion"_qs,1},{u"revision"_qs,QVariant::fromValue(quint64(1))},
        {u"configuredProvider"_qs,u"elevenlabs"_qs},{u"effectiveProvider"_qs,u"whisper_local"_qs},
        {u"credentialSource"_qs,u"unresolved"_qs},{u"statusCode"_qs,u"reload_required"_qs},
        {u"fallbackActive"_qs,true},{u"credentialCached"_qs,false},
        {u"environmentOverride"_qs,false},{u"canConfigure"_qs,true}};
}
class Fake final : public Transport {
public:
    int submissions = 0;
    quint64 fetchToken = 0;
    Operation operation = Operation::ReloadCredentials;
    void start() override {}
    void stop() override {}
    void fetch(const QString &, quint64 token) override { fetchToken = token; }
    void submit(const QString &, quint64, quint64, quint64, Operation kind, const QString &) override {
        ++submissions; operation = kind;
    }
    void ready(Client &client) {
        client.start(); Q_EMIT ownerChanged(u":1.2"_qs);
        Q_EMIT snapshotReply(u":1.2"_qs,fetchToken,true,false,snapshot());
    }
};
}
class Tests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void realMaskedEditorSubmitsAndClears() {
        Fake transport; Client client(transport); VoiceCredentialsModel model(client);
        transport.ready(client);
        QQmlEngine engine; engine.addImportPath(QStringLiteral(VOICE_TEST_QML_PATH));
        QQmlComponent component(&engine);
        component.loadFromModule(u"QindaQt.SettingsApp.Voice"_qs,u"VoiceCredentialSection"_qs);
        QVERIFY2(!component.isError(),qPrintable(component.errorString()));
        std::unique_ptr<QObject> root(component.createWithInitialProperties(
            {{u"configuration"_qs,QVariant::fromValue<QObject *>(&model)}}));
        QVERIFY2(root,qPrintable(component.errorString()));
        auto *entry = root->findChild<QObject *>(u"voiceApiKeyEntry"_qs);
        auto *save = root->findChild<QObject *>(u"voiceSaveApiKey"_qs);
        auto *reload = root->findChild<QObject *>(u"voiceReloadApiKey"_qs);
        QVERIFY(entry && save && reload);
        QCOMPARE(entry->property("echoMode").toInt(),2); // TextInput.Password
        QCOMPARE(entry->property("maximumLength").toInt(),512);
        QVERIFY(model.effectiveText().contains(u"whisper_local"_qs));
        QVERIFY(model.effectiveText().contains(u"fallback"_qs));
        entry->setProperty("text",u"test-only-dummy-key"_qs);
        QVERIFY(QMetaObject::invokeMethod(save,"clicked"));
        QVERIFY(entry->property("text").toString().isEmpty());
        QCOMPARE(transport.submissions,1);
        QCOMPARE(transport.operation,Operation::SaveElevenLabsKey);
        // Owner replacement purges an abandoned draft and refuses stale control.
        entry->setProperty("text",u"another-test-only-key"_qs);
        Q_EMIT transport.ownerChanged(u":1.3"_qs);
        QVERIFY(entry->property("text").toString().isEmpty());
        QVERIFY(!model.available());
        entry->setProperty("text",u"refused-test-only-key"_qs);
        QVERIFY(QMetaObject::invokeMethod(reload,"clicked"));
        QVERIFY(entry->property("text").toString().isEmpty());
        QCOMPARE(transport.submissions,1);
    }
    void reloadOverrideUnsupportedAndActionableErrors() {
        Fake transport; Client client(transport); VoiceCredentialsModel model(client);
        transport.ready(client);
        QVERIFY(model.reload());
        QCOMPARE(transport.operation,Operation::ReloadCredentials);
        client.stop(); transport.ready(client);
        auto state = snapshot(); state[u"credentialSource"_qs] = u"environment"_qs;
        state[u"environmentOverride"_qs] = true;
        state[u"revision"_qs] = QVariant::fromValue(quint64(2));
        client.refresh();
        Q_EMIT transport.snapshotReply(u":1.2"_qs,transport.fetchToken,true,false,state);
        QVERIFY(model.statusText().contains(u"overrides"_qs));
        state[u"statusCode"_qs] = u"keyring_unavailable"_qs;
        state[u"revision"_qs] = QVariant::fromValue(quint64(3));
        client.refresh();
        Q_EMIT transport.snapshotReply(u":1.2"_qs,transport.fetchToken,true,false,state);
        QVERIFY(model.statusText().contains(u"Unlock"_qs));
        client.refresh();
        Q_EMIT transport.snapshotReply(u":1.2"_qs,transport.fetchToken,false,true,{});
        QVERIFY(!model.available());
        QVERIFY(model.statusText().contains(u"does not support"_qs));
    }
};
QTEST_MAIN(Tests)
#include "tst_voice_credentials.moc"
