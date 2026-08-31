// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/portal/appearance_policy.h"
#include "qindaqt/services/portal/appearance_theme_catalog.h"
#include "qindaqt/services/portal/settings1_appearance_source.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"

#include <QtTest>

using namespace QindaQt::Services::Portal;
using namespace QindaQt::Services::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

class FakeSettingsTransport final : public SettingsTransport {
    Q_OBJECT
public:
    struct Request final {
        quint64 token = 0;
        QString owner;
        QStringList keys;
    };

    bool start(QString *error) override
    {
        started = true;
        if (error != nullptr) {
            error->clear();
        }
        return true;
    }
    void stop() override { started = false; }
    void requestSnapshot(quint64 token, const QString &owner,
                         const QStringList &keys) override
    {
        requests.append({token, owner, keys});
    }
    void commit(quint64, const QString &, const QString &, quint64,
                const QVariantList &) override
    {
    }
    void requestActivation() override { ++activations; }

    QList<Request> requests;
    int activations = 0;
    bool started = false;
};

namespace {

QVector<QindaQt::Themes::ThemeSpec> builtIns()
{
    QString error;
    const auto loaded = loadPortalAppearanceThemes(
        {QStringLiteral(QINDAQT_PORTAL_SOURCE_DIR "/data/themes")}, &error);
    if (!loaded.has_value()) {
        qFatal("%s", qPrintable(error));
    }
    return *loaded;
}

QVariantMap values(QString theme = QStringLiteral("qinda-dark"),
                   QString scheme = QStringLiteral("dark"),
                   bool highContrast = false)
{
    return {{QString::fromLatin1(kThemeSetting), std::move(theme)},
            {QString::fromLatin1(kColorSchemeSetting), std::move(scheme)},
            {QString::fromLatin1(kHighContrastSetting), highContrast},
            {QString::fromLatin1(kReducedTransparencySetting), false}};
}

QVariantMap snapshotWire(QString epoch, quint64 revision,
                         QVariantMap snapshotValues)
{
    QVariantMap sources;
    for (auto it = snapshotValues.cbegin(); it != snapshotValues.cend(); ++it) {
        sources.insert(it.key(), QStringLiteral("user-overrides"));
    }
    return {{QLatin1StringView(WireContract::FieldStatus),
             quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion),
             quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), std::move(epoch)},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues),
             std::move(snapshotValues)},
            {QLatin1StringView(WireContract::FieldSourceLayers), sources},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

} // namespace

class Settings1AppearanceSourceTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void publishesOnlyExactReadyLineageAndWithdrawsOnReplacement();
    void malformedProjectionWithdrawsCompleteTruth();
};

void Settings1AppearanceSourceTests::publishesOnlyExactReadyLineageAndWithdrawsOnReplacement()
{
    AppearancePolicyProjector projector(builtIns());
    FakeSettingsTransport transport;
    SettingsClient client(
        transport, AppearancePolicyProjector::scopedSettingsKeys(),
        {.requestTimeoutMilliseconds = 200,
         .debounceMilliseconds = 0,
         .retryMilliseconds = {10}});
    Settings1AppearanceSource source(client, projector);
    QSignalSpy changes(&source, &AppearanceSource::currentChanged);

    QVERIFY(source.start());
    QVERIFY(!source.current().has_value());
    QCOMPARE(transport.activations, 1);
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.20"));
    QTRY_COMPARE(transport.requests.size(), 1);
    const auto first = transport.requests.takeFirst();
    QCOMPARE(first.keys, AppearancePolicyProjector::scopedSettingsKeys());
    Q_EMIT transport.snapshotReceived(
        first.token, first.owner,
        snapshotWire(QStringLiteral("epoch-a"), 4, values()));
    QTRY_VERIFY(source.current().has_value());
    QCOMPARE(source.current()->owner, QStringLiteral(":1.20"));
    QCOMPARE(source.current()->epoch, QStringLiteral("epoch-a"));
    QCOMPARE(source.current()->revision, quint64(4));
    QCOMPARE(source.current()->policy.colorScheme,
             PortalColorScheme::PreferDark);

    Q_EMIT transport.ownerChanged(QStringLiteral(":1.21"));
    QVERIFY(!source.current().has_value());
    QVERIFY(!source.diagnostic().isEmpty());
    QTRY_COMPARE(transport.requests.size(), 1);

    // A late old-owner reply cannot restore stale truth.
    Q_EMIT transport.snapshotReceived(
        first.token, first.owner,
        snapshotWire(QStringLiteral("epoch-a"), 99,
                     values(QStringLiteral("qinda-light"),
                            QStringLiteral("light"))));
    QVERIFY(!source.current().has_value());

    const auto second = transport.requests.takeFirst();
    Q_EMIT transport.snapshotReceived(
        second.token, second.owner,
        snapshotWire(QStringLiteral("epoch-b"), 0,
                     values(QStringLiteral("qinda-light"),
                            QStringLiteral("light"))));
    QTRY_VERIFY(source.current().has_value());
    QCOMPARE(source.current()->owner, QStringLiteral(":1.21"));
    QCOMPARE(source.current()->epoch, QStringLiteral("epoch-b"));
    QCOMPARE(source.current()->policy.colorScheme,
             PortalColorScheme::PreferLight);
    QVERIFY(changes.size() >= 3);

    Q_EMIT transport.ownerChanged(QString{});
    QVERIFY(!source.current().has_value());
    source.stop();
    QVERIFY(!transport.started);
}

void Settings1AppearanceSourceTests::malformedProjectionWithdrawsCompleteTruth()
{
    AppearancePolicyProjector projector(builtIns());
    FakeSettingsTransport transport;
    SettingsClient client(
        transport, AppearancePolicyProjector::scopedSettingsKeys(),
        {.requestTimeoutMilliseconds = 200,
         .debounceMilliseconds = 0,
         .retryMilliseconds = {10}});
    Settings1AppearanceSource source(client, projector);
    QVERIFY(source.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.30"));
    QTRY_COMPARE(transport.requests.size(), 1);
    const auto first = transport.requests.takeFirst();
    Q_EMIT transport.snapshotReceived(
        first.token, first.owner,
        snapshotWire(QStringLiteral("epoch"), 1, values()));
    QTRY_VERIFY(source.current().has_value());

    Q_EMIT transport.settingsChanged(first.owner, QStringLiteral("epoch"), 2,
                                     {QString::fromLatin1(kThemeSetting)});
    QTRY_COMPARE(transport.requests.size(), 1);
    const auto malformed = transport.requests.takeFirst();
    Q_EMIT transport.snapshotReceived(
        malformed.token, malformed.owner,
        snapshotWire(QStringLiteral("epoch"), 2,
                     values(QStringLiteral("removed-theme"))));
    QTRY_VERIFY(!source.current().has_value());
    QVERIFY(source.diagnostic().contains(QStringLiteral("not installed")));

    // A later valid same-owner revision is the only way to republish.
    Q_EMIT transport.settingsChanged(first.owner, QStringLiteral("epoch"), 3,
                                     {QString::fromLatin1(kThemeSetting)});
    QTRY_COMPARE(transport.requests.size(), 1);
    const auto recovered = transport.requests.takeFirst();
    Q_EMIT transport.snapshotReceived(
        recovered.token, recovered.owner,
        snapshotWire(QStringLiteral("epoch"), 3,
                     values(QStringLiteral("qinda-high-contrast"))));
    QTRY_VERIFY(source.current().has_value());
    QCOMPARE(source.current()->policy.contrast, PortalContrast::PreferHigh);
}

QTEST_GUILESS_MAIN(Settings1AppearanceSourceTests)
#include "tst_settings1_appearance_source.moc"
