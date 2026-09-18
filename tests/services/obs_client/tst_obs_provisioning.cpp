// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/obs_client/obs_provisioning.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::Obs;

namespace {

WebSocketSettings settingsWith(const QString &password, int port = 4455) {
    WebSocketSettings settings;
    settings.password = password;
    settings.serverPort = port;
    return settings;
}

QJsonObject readObject(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

} // namespace

class ObsProvisioningTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void aGeneratedPasswordIsLongAndNotRepeated();
    void theWebSocketDocumentUsesObsWebSocketsOwnKeys();
    void installWritesTheThreeThingsOnlyUnderTheGivenRoot();
    void installRefusesSettingsThatWouldLeaveTheServerUnusable();
    void inspectNamesWhatIsMissingRatherThanJustFailing();
    void readingSettingsNeverReturnsThePassword();
    void theSceneCollectionLeavesTheConsoleBusesToTheBridge();
};

void ObsProvisioningTest::aGeneratedPasswordIsLongAndNotRepeated() {
    QSet<QString> seen;
    for (int attempt = 0; attempt < 32; ++attempt) {
        const QString password = generateWebSocketPassword();
        QCOMPARE(password.size(), 32);
        // URL-safe, no ambiguous glyphs: a user may have to retype it.
        for (const QChar character : password) {
            QVERIFY2(character.isLetterOrNumber(), qPrintable(password));
            QVERIFY(character != QLatin1Char('l') && character != QLatin1Char('I'));
            QVERIFY(character != QLatin1Char('0') && character != QLatin1Char('O'));
        }
        seen.insert(password);
    }
    QCOMPARE(seen.size(), 32);
}

void ObsProvisioningTest::theWebSocketDocumentUsesObsWebSocketsOwnKeys() {
    // These key names were read out of the installed obs-websocket.so, not
    // guessed; a renamed key would silently leave the server off.
    const QJsonObject document =
        webSocketConfigDocument(settingsWith(QStringLiteral("secret")));
    QVERIFY(document.value(QStringLiteral("server_enabled")).toBool());
    QCOMPARE(document.value(QStringLiteral("server_port")).toInt(), 4455);
    QCOMPARE(document.value(QStringLiteral("server_password")).toString(),
             QStringLiteral("secret"));
    QVERIFY(document.value(QStringLiteral("auth_required")).toBool());
    // first_load off, or obs-websocket shows its own welcome dialog and
    // generates a password that is not the one in the keyring.
    QVERIFY(!document.value(QStringLiteral("first_load")).toBool());
    QVERIFY(!document.value(QStringLiteral("alerts_enabled")).toBool());
}

void ObsProvisioningTest::installWritesTheThreeThingsOnlyUnderTheGivenRoot() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QString error;
    QVERIFY2(installProvisioning(root.path(), settingsWith(QStringLiteral("pw")),
                                 &error),
             qPrintable(error));

    const QString profile =
        QDir(root.path()).filePath(QStringLiteral("basic/profiles/QindaQt/basic.ini"));
    const QString collection =
        QDir(root.path()).filePath(QStringLiteral("basic/scenes/QindaQt.json"));
    const QString websocket = QDir(root.path())
                                  .filePath(QStringLiteral(
                                      "plugin_config/obs-websocket/config.json"));
    QVERIFY(QFile::exists(profile));
    QVERIFY(QFile::exists(collection));
    QVERIFY(QFile::exists(websocket));

    // Exactly three files; nothing else is written near the user's OBS.
    QStringList written;
    QDirIterator it(root.path(), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        written.append(QDir(root.path()).relativeFilePath(it.next()));
    }
    written.sort();
    QCOMPARE(written,
             (QStringList{QStringLiteral("basic/profiles/QindaQt/basic.ini"),
                          QStringLiteral("basic/scenes/QindaQt.json"),
                          QStringLiteral("plugin_config/obs-websocket/config.json")}));

    // The profile names itself and does not invent a stream key.
    QFile ini(profile);
    QVERIFY(ini.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(ini.readAll());
    QVERIFY(text.contains(QStringLiteral("Name=QindaQt")));
    QVERIFY(!text.contains(QStringLiteral("Key=")));
    QVERIFY(!text.contains(QStringLiteral("[Stream1]")));

    // Installing twice replaces rather than duplicating.
    QVERIFY(installProvisioning(root.path(), settingsWith(QStringLiteral("pw2")),
                                &error));
    QCOMPARE(readObject(websocket).value(QStringLiteral("server_password")).toString(),
             QStringLiteral("pw2"));
}

void ObsProvisioningTest::installRefusesSettingsThatWouldLeaveTheServerUnusable() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QString error;
    // Authentication on with no password: OBS would refuse every client and
    // the user would have no way to see why.
    QVERIFY(!installProvisioning(root.path(), settingsWith(QString()), &error));
    QVERIFY(!error.isEmpty());
    // An impossible port.
    QVERIFY(!installProvisioning(root.path(),
                                 settingsWith(QStringLiteral("pw"), 0), &error));
    QVERIFY(!installProvisioning(root.path(),
                                 settingsWith(QStringLiteral("pw"), 70000),
                                 &error));
    // And no root at all.
    QVERIFY(!installProvisioning(QString(), settingsWith(QStringLiteral("pw")),
                                 &error));
    // Nothing was written by any of those.
    QVERIFY(QDir(root.path()).entryList(QDir::NoDotAndDotDot | QDir::AllEntries)
                .isEmpty());
}

void ObsProvisioningTest::inspectNamesWhatIsMissingRatherThanJustFailing() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const WebSocketSettings expected = settingsWith(QStringLiteral("pw"));

    ProvisioningState state = inspectProvisioning(root.path(), expected);
    QVERIFY(!state.complete());
    QVERIFY(!state.profileInstalled);
    QVERIFY(!state.sceneCollectionInstalled);
    QVERIFY(!state.webSocketConfigured);
    // "install" and "repair" are different promises; the surface needs the
    // reason, not a boolean.
    QCOMPARE(state.problems.size(), 3);

    QString error;
    QVERIFY(installProvisioning(root.path(), expected, &error));
    state = inspectProvisioning(root.path(), expected);
    QVERIFY(state.complete());
    QVERIFY(state.problems.isEmpty());

    // A user who moved OBS to another port is told which port it is on.
    const ProvisioningState mismatched =
        inspectProvisioning(root.path(), settingsWith(QStringLiteral("pw"), 4466));
    QVERIFY(!mismatched.complete());
    QVERIFY(mismatched.webSocketConfigured);
    QVERIFY(!mismatched.webSocketPortMatches);
    QCOMPARE(mismatched.problems.size(), 1);
    QVERIFY(mismatched.problems.first().contains(QStringLiteral("4455")));
    QVERIFY(mismatched.problems.first().contains(QStringLiteral("4466")));
}

void ObsProvisioningTest::readingSettingsNeverReturnsThePassword() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QString error;
    QVERIFY(installProvisioning(root.path(),
                                settingsWith(QStringLiteral("a-real-secret")),
                                &error));
    bool found = false;
    const WebSocketSettings read = readWebSocketSettings(root.path(), &found);
    QVERIFY(found);
    QCOMPARE(read.serverPort, 4455);
    QVERIFY(read.serverEnabled);
    QVERIFY(read.authRequired);
    // AGENT-GUARD: a surface that shows the port must not come holding the
    // secret as a side effect.
    QVERIFY(read.password.isEmpty());

    bool missing = true;
    const WebSocketSettings absent = readWebSocketSettings(
        QDir(root.path()).filePath(QStringLiteral("nowhere")), &missing);
    QVERIFY(!missing);
    QVERIFY(!absent.serverEnabled);
}

void ObsProvisioningTest::theSceneCollectionLeavesTheConsoleBusesToTheBridge() {
    const QJsonObject collection = sceneCollectionDocument();
    QCOMPARE(collection.value(QStringLiteral("name")).toString(),
             QStringLiteral("QindaQt"));
    const QJsonArray sources = collection.value(QStringLiteral("sources")).toArray();
    QStringList ids;
    for (const QJsonValue &value : sources) {
        ids.append(value.toObject().value(QStringLiteral("id")).toString());
    }
    QVERIFY(ids.contains(QStringLiteral("pipewire-desktop-capture-source")));
    QVERIFY(ids.contains(QStringLiteral("pulse_input_capture")));
    QVERIFY(ids.contains(QStringLiteral("scene")));
    // AGENT-GUARD: the bridge creates and removes the console sources to
    // match Audio1 (ADR-0190). A scene collection that also declared them
    // would fight the bridge on every console change.
    QVERIFY(!ids.contains(QStringLiteral("qindaqt_console_bus")));
    QVERIFY(!ids.contains(QStringLiteral("qindaqt_console_strip")));
}

QTEST_APPLESS_MAIN(ObsProvisioningTest)
#include "tst_obs_provisioning.moc"
