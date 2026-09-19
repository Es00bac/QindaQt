// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/session/desktop_controls/settings1_screensaver_preferences.h>

#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>

#include <QtTest>

#include <memory>

using namespace QindaQt::Session::DesktopControls;
using namespace QindaQt::Services::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

namespace {

constexpr auto kSaverKey = "power.screensaver";
constexpr auto kMinutesKey = "power.screensaverMinutes";

class FakeSettingsTransport final : public SettingsTransport {
    Q_OBJECT

public:
    bool start(QString *error) override
    {
        if (error != nullptr) {
            error->clear();
        }
        return true;
    }
    void stop() override {}

    void requestSnapshot(quint64 token, const QString &owner, const QStringList &) override
    {
        QMetaObject::invokeMethod(
            this,
            [this, token, owner] {
                Q_EMIT snapshotReceived(token, owner, snapshotWire(++m_revision));
            },
            Qt::QueuedConnection);
    }

    void commit(quint64, const QString &, const QString &, quint64,
                const QVariantList &) override
    {
        ++m_commits;
    }
    void requestActivation() override {}

    [[nodiscard]] int commits() const noexcept { return m_commits; }

    void announceOwner()
    {
        QMetaObject::invokeMethod(
            this, [this] { Q_EMIT ownerChanged(QStringLiteral(":1.12")); },
            Qt::QueuedConnection);
    }

    void setValue(const char *key, const QVariant &value)
    {
        m_values.insert(QString::fromLatin1(key), value);
    }

    [[nodiscard]] QVariantMap snapshotWire(quint64 revision) const
    {
        QVariantMap layers;
        for (auto it = m_values.constBegin(); it != m_values.constEnd(); ++it) {
            layers.insert(it.key(), QStringLiteral("user-overrides"));
        }
        return {{QLatin1StringView(WireContract::FieldStatus),
                 quint32(SettingsWireStatus::Applied)},
                {QLatin1StringView(WireContract::FieldWireSchemaVersion),
                 WireContract::WireSchemaVersion},
                {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
                {QLatin1StringView(WireContract::FieldEpoch), m_epoch},
                {QLatin1StringView(WireContract::FieldRevision), revision},
                {QLatin1StringView(WireContract::FieldValues), m_values},
                {QLatin1StringView(WireContract::FieldSourceLayers), layers},
                {QLatin1StringView(WireContract::FieldMessage), QString{}}};
    }

private:
    QString m_epoch = QStringLiteral("epoch-12");
    quint64 m_revision = 0;
    int m_commits = 0;
    QVariantMap m_values{{QString::fromLatin1(kSaverKey), QStringLiteral("none")},
                         {QString::fromLatin1(kMinutesKey),
                          QVariant::fromValue<qint64>(5)}};
};

} // namespace

class Settings1ScreensaverPreferencesTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void noOwnerYetStartsDisabled();
    void persistedPairMapsToBoundedPreferences();
    void unknownTokenNeverBecomesAProgramName();
    void persistedMinutesAreClamped();
    void knownSaversCarryTheirFixedCommandLine();
    void onlyQmlSaversReachTheLockScreen();
    void valueChangeIsSignalledOnce();
    void refreshNeverWrites();

private:
    void rebuildClient();

    std::unique_ptr<FakeSettingsTransport> m_transport;
    std::unique_ptr<SettingsClient> m_client;
};

void Settings1ScreensaverPreferencesTest::rebuildClient() {
    // AGENT-GUARD: destroy the client before the transport it borrows;
    // reversing this order stops a client against freed transport memory.
    m_client.reset();
    m_transport = std::make_unique<FakeSettingsTransport>();
    m_client = std::make_unique<SettingsClient>(
        *m_transport, Settings1ScreensaverPreferences::scopedKeys());
}

void Settings1ScreensaverPreferencesTest::noOwnerYetStartsDisabled() {
    rebuildClient();
    QString error;
    QVERIFY(m_client->start(&error));

    Settings1ScreensaverPreferences preferences(*m_client);
    // AGENT-GUARD: unlike display-off, an unconfirmed preference starts no
    // program at all. Guessing here would run a saver the user never chose.
    QCOMPARE(preferences.currentPreferences(), ScreensaverPreferences{});
    QVERIFY(!preferences.currentPreferences().enabled());
    QVERIFY(preferences.currentPreferences().program().isEmpty());
    QCOMPARE(Settings1ScreensaverPreferences::scopedKeys(),
             (QStringList{QString::fromLatin1(kSaverKey),
                          QString::fromLatin1(kMinutesKey)}));
}

void Settings1ScreensaverPreferencesTest::persistedPairMapsToBoundedPreferences() {
    rebuildClient();
    QString error;
    QVERIFY(m_client->start(&error));
    m_transport->setValue(kSaverKey, QStringLiteral("qinda-patrol"));
    m_transport->setValue(kMinutesKey, QVariant::fromValue<qint64>(25));

    Settings1ScreensaverPreferences preferences(*m_client);
    m_transport->announceOwner();
    QTRY_VERIFY(preferences.currentPreferences().enabled());
    QCOMPARE(preferences.currentPreferences().saver, QStringLiteral("qinda-patrol"));
    QCOMPARE(preferences.currentPreferences().minutes, 25);
}

void Settings1ScreensaverPreferencesTest::unknownTokenNeverBecomesAProgramName() {
    rebuildClient();
    QString error;
    QVERIFY(m_client->start(&error));
    m_transport->setValue(kSaverKey, QStringLiteral("/usr/bin/anything"));
    m_transport->setValue(kMinutesKey, QVariant::fromValue<qint64>(9));

    Settings1ScreensaverPreferences preferences(*m_client);
    m_transport->announceOwner();
    QTRY_COMPARE(preferences.currentPreferences().minutes, 9);
    QCOMPARE(preferences.currentPreferences().saver,
             ScreensaverPreferences::noneToken());
    QVERIFY(preferences.currentPreferences().program().isEmpty());
}

void Settings1ScreensaverPreferencesTest::persistedMinutesAreClamped() {
    rebuildClient();
    QString error;
    QVERIFY(m_client->start(&error));
    m_transport->setValue(kSaverKey, QStringLiteral("circuit-reef"));
    m_transport->setValue(kMinutesKey, QVariant::fromValue<qint64>(100000));

    Settings1ScreensaverPreferences preferences(*m_client);
    m_transport->announceOwner();
    QTRY_COMPARE(preferences.currentPreferences().minutes,
                 ScreensaverPreferences::maximumTimeoutMinutes());

    QCOMPARE(ScreensaverPreferences::fromPersisted(QStringLiteral("circuit-reef"), 0)
                 .minutes,
             1);
    QCOMPARE(ScreensaverPreferences::fromPersisted(QStringLiteral("circuit-reef"), -5)
                 .minutes,
             1);
}

void Settings1ScreensaverPreferencesTest::knownSaversCarryTheirFixedCommandLine() {
    // AGENT-CONTRACT: every saver is started with --screensaver, which each
    // one documents as covering every connected output, and with whatever
    // extra flag stops it doing something an unattended screen should not do.
    // These are the launcher's only command lines, so they live here with the
    // preference and are asserted as a set.
    const QList<QPair<QString, QStringList>> expected{
        {QStringLiteral("qinda-patrol"),
         {QStringLiteral("--screensaver"), QStringLiteral("--no-metrics")}},
        {QStringLiteral("circuit-reef"),
         {QStringLiteral("--screensaver"), QStringLiteral("--private")}},
        {QStringLiteral("prism-circuit"),
         {QStringLiteral("--screensaver"), QStringLiteral("--mute")}},
        {QStringLiteral("prism-brawl"),
         {QStringLiteral("--screensaver"), QStringLiteral("--mute")}},
        {QStringLiteral("starward"), {QStringLiteral("--screensaver")}},
    };
    for (const auto &[token, arguments] : expected) {
        const auto preferences = ScreensaverPreferences::fromPersisted(token, 5);
        QCOMPARE(preferences.program(), token);
        QCOMPARE(preferences.arguments(), arguments);
    }

    // The offered set is exactly the reserved token plus those five: a saver
    // the schema does not allow could never be persisted, and one missing here
    // could never be started.
    QStringList offered{ScreensaverPreferences::noneToken()};
    for (const auto &[token, arguments] : expected) {
        offered.append(token);
    }
    QCOMPARE(ScreensaverPreferences::knownSavers(), offered);

    const auto none =
        ScreensaverPreferences::fromPersisted(ScreensaverPreferences::noneToken(), 5);
    QVERIFY(none.program().isEmpty());
    QVERIFY(none.arguments().isEmpty());
}

void Settings1ScreensaverPreferencesTest::onlyQmlSaversReachTheLockScreen() {
    // AGENT-GUARD: the greeter draws a saver by importing its QML module
    // (ADR-0216). The three SDL/OpenGL savers ship none, and saying otherwise
    // would hand the locker a wallpaper plugin with nothing to draw.
    QVERIFY(ScreensaverPreferences::showsOnLockScreen(QStringLiteral("qinda-patrol")));
    QVERIFY(ScreensaverPreferences::showsOnLockScreen(QStringLiteral("circuit-reef")));
    QVERIFY(!ScreensaverPreferences::showsOnLockScreen(QStringLiteral("prism-circuit")));
    QVERIFY(!ScreensaverPreferences::showsOnLockScreen(QStringLiteral("prism-brawl")));
    QVERIFY(!ScreensaverPreferences::showsOnLockScreen(QStringLiteral("starward")));
    QVERIFY(!ScreensaverPreferences::showsOnLockScreen(
        ScreensaverPreferences::noneToken()));
}

void Settings1ScreensaverPreferencesTest::valueChangeIsSignalledOnce() {
    rebuildClient();
    QString error;
    QVERIFY(m_client->start(&error));
    m_transport->setValue(kSaverKey, QStringLiteral("qinda-patrol"));

    Settings1ScreensaverPreferences preferences(*m_client);
    QSignalSpy spy(&preferences, &ScreensaverPreferencesProvider::preferencesChanged);
    m_transport->announceOwner();
    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<ScreensaverPreferences>().saver,
             QStringLiteral("qinda-patrol"));

    // An unchanged snapshot stays quiet: the launcher must not restart a
    // running saver because settings re-published the same pair.
    m_client->refresh();
    QTRY_VERIFY(m_client->snapshot().has_value());
    QCOMPARE(spy.count(), 1);
}

void Settings1ScreensaverPreferencesTest::refreshNeverWrites() {
    rebuildClient();
    QString error;
    QVERIFY(m_client->start(&error));

    Settings1ScreensaverPreferences preferences(*m_client);
    preferences.refresh();
    // The provider is read-only: refresh only re-reads, it never commits.
    QCOMPARE(m_transport->commits(), 0);
}

QTEST_MAIN(Settings1ScreensaverPreferencesTest)
#include "tst_settings1_screensaver_preferences.moc"
