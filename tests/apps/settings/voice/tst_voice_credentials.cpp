// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_voice/voice_credentials_model.h>
#include <qindaqt/services/voice_configuration/voice_configuration_client.h>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlPropertyMap>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QDir>
#include <QFileInfo>
#include <qindaqt/design_tokens/token_facade.h>
#include <qindaqt/themes/theme_loader.h>
#include <QtTest/QTest>
#include <memory>
Q_IMPORT_QML_PLUGIN(QindaQtSettingsVoicePlugin)
using namespace QindaQt::Services::VoiceConfiguration;
using QindaQt::Apps::SettingsVoice::VoiceCredentialsModel;
using namespace Qt::StringLiterals;
namespace {
QVariantMap snapshot() {
    return {{u"schemaVersion"_s,1},{u"revision"_s,QVariant::fromValue(quint64(1))},
        {u"configuredProvider"_s,u"elevenlabs"_s},{u"effectiveProvider"_s,u"whisper_local"_s},
        {u"credentialSource"_s,u"unresolved"_s},{u"statusCode"_s,u"reload_required"_s},
        {u"fallbackActive"_s,true},{u"credentialCached"_s,false},
        {u"environmentOverride"_s,false},{u"canConfigure"_s,true}};
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
        client.start(); Q_EMIT ownerChanged(u":1.2"_s);
        Q_EMIT snapshotReply(u":1.2"_s,fetchToken,true,false,snapshot());
    }
};
}
class Tests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void realMaskedEditorSubmitsAndClears_data() {
        QTest::addColumn<bool>("disk");
        if (qEnvironmentVariableIsEmpty("QINDAQT_VOICE_STAGED_MODULE"))
            QTest::newRow("compiled-resource") << false;
        QTest::newRow("disk-source") << true;
    }
    void realMaskedEditorSubmitsAndClears() {
        QFETCH(bool,disk);
        Fake transport; Client client(transport); VoiceCredentialsModel model(client);
        transport.ready(client);
        QQmlEngine engine; engine.addImportPath(QStringLiteral(VOICE_TEST_QML_PATH));
        QQmlComponent registration(&engine);
        registration.setData("import QtQuick\nimport QindaQt.Tokens 1.0\nQtObject { property int revision: Tokens.qstRevision }", QUrl(u"inline:voice-tokens.qml"_s));
        QTRY_VERIFY_WITH_TIMEOUT(registration.status()!=QQmlComponent::Loading,5000);
        std::unique_ptr<QObject> registered(registration.create());
        QVERIFY2(registered,qPrintable(registration.errorString()));
        auto *tokens=engine.singletonInstance<QindaQt::DesignTokens::TokenFacade *>("QindaQt.Tokens","Tokens");
        QVERIFY(tokens);
        const auto theme=QindaQt::Themes::ThemeLoader::fromFile(
            QDir(QFileInfo(QStringLiteral(VOICE_SOURCE_QML_PATH)).absolutePath())
                .filePath(u"../../../../../data/themes/qinda-dark.json"_s));
        QVERIFY(theme.ok); QString error; QVERIFY2(tokens->publish(theme.theme,{},&error),qPrintable(error));
        QTest::failOnWarning(QRegularExpression(u".*"_s));
        QQmlComponent component(&engine);
        const QString staged=qEnvironmentVariable("QINDAQT_VOICE_STAGED_MODULE");
        const QString pagePath=staged.isEmpty()
            ? QDir(QFileInfo(QStringLiteral(VOICE_SOURCE_QML_PATH)).absolutePath()).filePath(u"VoicePage.qml"_s)
            : staged+u"/qml/VoicePage.qml"_s;
        if (disk) component.loadUrl(QUrl::fromLocalFile(pagePath));
        else component.loadFromModule(u"QindaQt.SettingsApp.Voice"_s,u"VoicePage"_s);
        QTRY_VERIFY_WITH_TIMEOUT(component.status()!=QQmlComponent::Loading,5000);
        QVERIFY2(!component.isError(),qPrintable(component.errorString()));
        std::unique_ptr<QQmlPropertyMap> pageState(QQmlPropertyMap::create());
        for (const auto *key : {"preferenceLoading","preferenceSaving","preferenceUnavailable",
             "canRetryProvider","canEditPreference","canChooseProvider","providerBusy",
             "canStartDictation","canCancelDictation"}) pageState->insert(QString::fromLatin1(key),false);
        for (const auto *key : {"serviceAvailable","draftVoiceInputEnabled","preferenceReady",
             "voiceInputEnabled","draftPanelTranscriptEnabled","shortcutsArmed"})
            pageState->insert(QString::fromLatin1(key),true);
        for (const auto *key : {"preferenceStatusText","serviceStatusText","preferenceErrorText",
             "routeLabel","providerErrorText","lastText"}) pageState->insert(QString::fromLatin1(key),QString{});
        pageState->insert(u"providerLabel"_s,u"ElevenLabs"_s);
        pageState->insert(u"sessionStateText"_s,u"Ready"_s);
        pageState->insert(u"dictationShortcut"_s,u"F5"_s);
        pageState->insert(u"commandShortcut"_s,u"F6"_s);
        pageState->insert(u"microphoneLabel"_s,u"Test microphone"_s);
        pageState->insert(u"languageLabel"_s,u"English"_s);
        pageState->insert(u"capabilityRows"_s,QVariantList{});
        pageState->insert(u"providerRows"_s,QVariantList{QVariantMap{
            {u"label"_s,u"ElevenLabs"_s},{u"providerId"_s,u"elevenlabs"_s},{u"current"_s,true}}});
        pageState->insert(u"configuration"_s,QVariant::fromValue<QObject *>(&model));
        QQuickWindow window;
        std::unique_ptr<QObject> root(component.createWithInitialProperties(
            {{u"voiceSettings"_s,QVariant::fromValue<QObject *>(pageState.get())}}));
        QVERIFY2(root,qPrintable(component.errorString()));
        auto *entry = root->findChild<QObject *>(u"voiceApiKeyEntry"_s);
        auto *save = root->findChild<QObject *>(u"voiceSaveApiKey"_s);
        auto *reload = root->findChild<QObject *>(u"voiceReloadApiKey"_s);
        QVERIFY(entry && save && reload);
        auto *serviceCard=root->findChild<QObject *>(u"voiceServiceState"_s); QVERIFY(serviceCard);
        QVERIFY(serviceCard->property("message").toString().contains(u"Whisper (local)"_s));
        auto *item=qobject_cast<QQuickItem *>(root.get()); QVERIFY(item);
        item->setParentItem(window.contentItem()); item->setSize(QSizeF(360,760));
        window.resize(360,760); window.show();
        QTRY_VERIFY(reload->property("y").toReal()>save->property("y").toReal());
        QVERIFY(save->property("width").toReal()<=360);
        QVERIFY(reload->property("width").toReal()<=360);
        const QString capture=qEnvironmentVariable("QINDAQT_VOICE_CAPTURE_PREFIX");
        if (!capture.isEmpty()) {
            QTest::qWait(30);
            QVERIFY(window.grabWindow().save(capture+(disk ? u"-disk-top.png"_s : u"-compiled-top.png"_s)));
            auto *viewport=root->findChild<QQuickItem *>(u"voiceFormViewport"_s); QVERIFY(viewport);
            auto *field=qobject_cast<QQuickItem *>(entry); QVERIFY(field);
            viewport->setProperty("contentY",field->mapToItem(viewport,QPointF{}).y()-100);
            QTest::qWait(30);
            QVERIFY(window.grabWindow().save(capture+(disk ? u"-disk-credentials.png"_s : u"-compiled-credentials.png"_s)));
        }
        QCOMPARE(entry->property("echoMode").toInt(),2); // TextInput.Password
        QCOMPARE(entry->property("maximumLength").toInt(),512);
        QVERIFY(model.effectiveText().contains(u"Whisper (local)"_s));
        QVERIFY(model.effectiveText().contains(u"fallback"_s));
        entry->setProperty("text",u"test-only-dummy-key"_s);
        QVERIFY(QMetaObject::invokeMethod(save,"clicked"));
        QVERIFY(entry->property("text").toString().isEmpty());
        QCOMPARE(transport.submissions,1);
        QCOMPARE(transport.operation,Operation::SaveElevenLabsKey);
        // Owner replacement purges an abandoned draft and refuses stale control.
        entry->setProperty("text",u"another-test-only-key"_s);
        Q_EMIT transport.ownerChanged(u":1.3"_s);
        QVERIFY(entry->property("text").toString().isEmpty());
        QVERIFY(!model.available());
        entry->setProperty("text",u"refused-test-only-key"_s);
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
        auto state = snapshot(); state[u"credentialSource"_s] = u"environment"_s;
        state[u"environmentOverride"_s] = true;
        state[u"revision"_s] = QVariant::fromValue(quint64(2));
        client.refresh();
        Q_EMIT transport.snapshotReply(u":1.2"_s,transport.fetchToken,true,false,state);
        QVERIFY(model.statusText().contains(u"overrides"_s));
        state[u"statusCode"_s] = u"keyring_unavailable"_s;
        state[u"revision"_s] = QVariant::fromValue(quint64(3));
        client.refresh();
        Q_EMIT transport.snapshotReply(u":1.2"_s,transport.fetchToken,true,false,state);
        QVERIFY(model.statusText().contains(u"Unlock"_s));
        client.refresh();
        Q_EMIT transport.snapshotReply(u":1.2"_s,transport.fetchToken,false,true,{});
        QVERIFY(!model.available());
        QVERIFY(model.statusText().contains(u"does not support"_s));
    }
};
QTEST_MAIN(Tests)
#include "tst_voice_credentials.moc"
