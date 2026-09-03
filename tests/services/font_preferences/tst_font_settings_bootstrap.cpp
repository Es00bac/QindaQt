// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/font_preferences/font_settings_bootstrap.h"
#include "qindaqt/services/font_preferences/font_settings_bridge.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QtTest>

using namespace QindaQt::Services::FontPreferences;
using namespace QindaQt::Services::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

namespace {

// AGENT-NOTE: The auto-answering fake emits the snapshot reply synchronously
// from requestSnapshot(); the client has already recorded the request, so the
// nested direct emission is processed exactly like an asynchronous reply.
class AutoTransport final : public SettingsTransport {
    Q_OBJECT
public:
    bool start(QString *error) override
    {
        if (!startSucceeds) {
            if (error != nullptr) *error = QStringLiteral("transport unavailable");
            return false;
        }
        return true;
    }
    void stop() override {}
    void requestSnapshot(quint64 token, const QString &owner, const QStringList &keys) override
    {
        QVariantMap values;
        QVariantMap sources;
        for (const QString &key : keys) {
            values.insert(key, m_values.value(key));
            sources.insert(key, QStringLiteral("user-overrides"));
        }
        Q_EMIT snapshotReceived(token, owner,
                                {{QLatin1StringView(WireContract::FieldStatus),
                                  quint32(SettingsWireStatus::Applied)},
                                 {QLatin1StringView(WireContract::FieldWireSchemaVersion),
                                  WireContract::WireSchemaVersion},
                                 {QLatin1StringView(WireContract::FieldSettingsSchemaVersion),
                                  quint32(2)},
                                 {QLatin1StringView(WireContract::FieldEpoch),
                                  QStringLiteral("epoch-1")},
                                 {QLatin1StringView(WireContract::FieldRevision), quint64(3)},
                                 {QLatin1StringView(WireContract::FieldValues), values},
                                 {QLatin1StringView(WireContract::FieldSourceLayers), sources},
                                 {QLatin1StringView(WireContract::FieldMessage), QString{}}});
    }
    void commit(quint64, const QString &, const QString &, quint64,
                const QVariantList &) override {}
    void requestActivation() override
    {
        if (!m_ownerEmitted) {
            m_ownerEmitted = true;
            Q_EMIT ownerChanged(QStringLiteral(":1.70"));
        }
    }

    QVariantMap m_values;
    bool startSucceeds = true;
    bool m_ownerEmitted = false;
};

QVariantMap confirmedFontsValues()
{
    return {{QStringLiteral("fonts.family"), QStringLiteral("Liberation Mono")},
            {QStringLiteral("fonts.monospaceFamily"), QStringLiteral("Liberation Mono")},
            {QStringLiteral("fonts.pointSize"), 13.5},
            {QStringLiteral("fonts.antialiasing"), true},
            {QStringLiteral("fonts.hinting"), QStringLiteral("full")},
            {QStringLiteral("fonts.subpixelOrder"), QStringLiteral("vrgb")}};
}

} // namespace

class FontSettingsBootstrapTests final : public QObject {
    Q_OBJECT
private slots:
    void applyPreferencesChangesDefaultFont();
    void applyPreferencesRejectsInvalidPreferences();
    void readConfirmedPreferencesDecodesSnapshot();
    void readConfirmedPreferencesFailsClosedOnStartFailure();
    void readConfirmedPreferencesTimesOutWithoutOwner();
    void applyFromSessionSettingsWithoutBusChangesNothing();
};

void FontSettingsBootstrapTests::applyPreferencesChangesDefaultFont()
{
    const QFont before = QGuiApplication::font();
    FontPreferences preferences;
    preferences.setFamily(QStringLiteral("QindaQt Bootstrap Probe"));
    preferences.setPointSize(14.0);
    preferences.setHinting(FontHinting::None);
    preferences.setAntialiasing(FontAntialiasing::None);

    QVERIFY(FontSettingsBootstrap::applyPreferences(*qGuiApp, preferences));
    const QFont after = QGuiApplication::font();
    QCOMPARE(after.family(), QStringLiteral("QindaQt Bootstrap Probe"));
    QCOMPARE(after.pointSizeF(), 14.0);
    QCOMPARE(after.hintingPreference(), QFont::PreferNoHinting);
    QVERIFY(after.styleStrategy() & QFont::NoAntialias);
    QVERIFY(after != before);
}

void FontSettingsBootstrapTests::applyPreferencesRejectsInvalidPreferences()
{
    const QFont before = QGuiApplication::font();
    FontPreferences preferences;
    preferences.setFamily(QString(QChar(0x01)));
    QVERIFY(!preferences.isValid());

    QString diagnostic;
    QVERIFY(!FontSettingsBootstrap::applyPreferences(*qGuiApp, preferences, &diagnostic));
    QVERIFY(!diagnostic.isEmpty());
    QCOMPARE(QGuiApplication::font(), before);
}

void FontSettingsBootstrapTests::readConfirmedPreferencesDecodesSnapshot()
{
    AutoTransport transport;
    transport.m_values = confirmedFontsValues();
    SettingsClient client(transport, FontSettingsBridge::scopedKeys(),
                          {.requestTimeoutMilliseconds = 200,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});

    const auto preferences = FontSettingsBootstrap::readConfirmedPreferences(client, 1'000);
    QVERIFY(preferences.has_value());
    QCOMPARE(preferences->family(), QStringLiteral("Liberation Mono"));
    QCOMPARE(preferences->pointSize(), 13.5);
    QCOMPARE(preferences->hinting(), FontHinting::Full);
    QCOMPARE(preferences->subpixelOrder(), FontSubpixelOrder::Vrgb);
}

void FontSettingsBootstrapTests::readConfirmedPreferencesFailsClosedOnStartFailure()
{
    AutoTransport transport;
    transport.startSucceeds = false;
    SettingsClient client(transport, FontSettingsBridge::scopedKeys(),
                          {.requestTimeoutMilliseconds = 200,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    QString diagnostic;
    const auto preferences =
        FontSettingsBootstrap::readConfirmedPreferences(client, 1'000, &diagnostic);
    QVERIFY(!preferences.has_value());
    QVERIFY(!diagnostic.isEmpty());
}

void FontSettingsBootstrapTests::readConfirmedPreferencesTimesOutWithoutOwner()
{
    AutoTransport transport;
    transport.m_ownerEmitted = true; // requestActivation never gains an owner
    SettingsClient client(transport, FontSettingsBridge::scopedKeys(),
                          {.requestTimeoutMilliseconds = 200,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    QElapsedTimer timer;
    timer.start();
    const auto preferences = FontSettingsBootstrap::readConfirmedPreferences(client, 150);
    QVERIFY(!preferences.has_value());
    QVERIFY(timer.elapsed() < 5'000);
}

void FontSettingsBootstrapTests::applyFromSessionSettingsWithoutBusChangesNothing()
{
    // AGENT-CONTRACT: Ground rule 6 forbids host buses in tests. Point the
    // session bus at an absent address so the production composition proves
    // its fail-closed guard without touching any real service.
    qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent/qindaqt-absent-session-bus");
    const QFont before = QGuiApplication::font();

    QString diagnostic;
    QVERIFY(!FontSettingsBootstrap::applyFromSessionSettings(*qGuiApp, &diagnostic));
    QCOMPARE(QGuiApplication::font(), before);
}

QTEST_MAIN(FontSettingsBootstrapTests)
#include "tst_font_settings_bootstrap.moc"
