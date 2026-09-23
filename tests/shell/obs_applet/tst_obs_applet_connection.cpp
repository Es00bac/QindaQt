// SPDX-License-Identifier: LGPL-3.0-or-later
#include "obsappletconnection.h"

#include <qindaqt/services/obs_client/obs_client.h>
#include <qindaqt/services/obs_client/obs_secret_store.h>
#include <qindaqt/services/streaming_preferences/streaming_preferences.h>

#include <QtTest>

using namespace QindaQt;

namespace {
class FakeTransport final : public Obs::ObsTransport {
    Q_OBJECT
public:
    QStringList opened;
    int closes = 0;
    void open(const QString &url) override { opened.append(url); }
    void close() override { ++closes; }
    void sendText(const QString &) override {}
    [[nodiscard]] bool isOpen() const override { return false; }
};
class FakeSecrets final : public Obs::ObsSecretStore {
    Q_OBJECT
public:
    mutable int reads = 0;
    std::optional<QString> stored = QStringLiteral("private-password");
    [[nodiscard]] std::optional<QString> password(QString *error) const override {
        ++reads;
        if (error) error->clear();
        return stored;
    }
    [[nodiscard]] bool setPassword(const QString &, QString *) override { return false; }
};
class FakePreferences final : public Services::StreamingPreferences::StreamingPreferences {
    Q_OBJECT
public:
    bool loaded = false;
    bool automatic = true;
    int port = 4455;
    [[nodiscard]] bool isLoaded() const override { return loaded; }
    [[nodiscard]] int webSocketPort() const override { return port; }
    [[nodiscard]] bool autoConnect() const override { return automatic; }
    [[nodiscard]] bool startObsAtLogin() const override { return false; }
    bool setWebSocketPort(int) override { return false; }
    bool setAutoConnect(bool) override { return false; }
    bool setStartObsAtLogin(bool) override { return false; }
};
} // namespace

class ObsAppletConnectionTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void baselineAutoConnectAndActivePortGateEveryAttempt();
};

void ObsAppletConnectionTest::baselineAutoConnectAndActivePortGateEveryAttempt() {
    FakeTransport transport;
    Obs::ObsClient client(transport);
    FakeSecrets secrets;
    FakePreferences preferences;
    std::optional<int> activePort = 4455;
    Shell::ObsAppletConnection connection(client, secrets, preferences,
                                          [&activePort] { return activePort; });
    QVERIFY(!connection.reconcile());
    QCOMPARE(secrets.reads, 0);
    QVERIFY(transport.opened.isEmpty());
    preferences.loaded = true;
    preferences.automatic = false;
    QVERIFY(!connection.reconcile());
    QCOMPARE(secrets.reads, 0);
    preferences.automatic = true;
    preferences.port = 4466;
    QVERIFY(!connection.reconcile()); // OBS still listens on 4455
    QCOMPARE(secrets.reads, 0);
    activePort = 4466;
    QVERIFY(connection.reconcile());
    QCOMPARE(transport.opened, QStringList{QStringLiteral("ws://127.0.0.1:4466")});
    QCOMPARE(secrets.reads, 1);
    QVERIFY(connection.reconcile());
    QCOMPARE(transport.opened.size(), 1); // watch is not a second launcher
    preferences.automatic = false;
    QVERIFY(!connection.reconcile());
    QCOMPARE(transport.opened.size(), 1);
    preferences.automatic = true;
    activePort = 4455;
    preferences.port = 4455;
    QVERIFY(connection.reconcile());
    QCOMPARE(transport.opened.size(), 2);
    QCOMPARE(transport.opened.constLast(), QStringLiteral("ws://127.0.0.1:4455"));
    preferences.port = 4466;
    QVERIFY(!connection.reconcile());
    QCOMPARE(transport.opened.size(), 2);
    activePort = 4466;
    secrets.stored.reset();
    QVERIFY(!connection.reconcile());
    QCOMPARE(transport.opened.size(), 2);
}

QTEST_GUILESS_MAIN(ObsAppletConnectionTest)
#include "tst_obs_applet_connection.moc"
