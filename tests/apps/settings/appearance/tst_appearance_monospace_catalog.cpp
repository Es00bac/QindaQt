// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_settings_model.h"
#include "qindaqt/apps/settings_appearance/appearance_values.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/themes/theme_loader.h"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QtTest>

using namespace QindaQt::Apps::SettingsAppearance;
using namespace QindaQt::Services::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

namespace {
class SnapshotTransport final : public SettingsTransport {
public:
    bool start(QString *) override { return true; }
    void stop() override {}
    void requestSnapshot(quint64 token, const QString &owner,
                         const QStringList &) override
    {
        snapshotToken = token;
        snapshotOwner = owner;
    }
    void commit(quint64, const QString &, const QString &, quint64,
                const QVariantList &) override {}
    void requestActivation() override {}

    quint64 snapshotToken = 0;
    QString snapshotOwner;
};

QVector<QindaQt::Themes::ThemeSpec> fixtureThemes()
{
    QVector<QindaQt::Themes::ThemeSpec> themes;
    for (const auto *file : {"/data/themes/qinda-dark.json",
                             "/data/themes/qinda-light.json",
                             "/data/themes/qinda-high-contrast.json"}) {
        const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
            QStringLiteral(QINDAQT_SOURCE_DIR) + QLatin1String(file));
        if (loaded.ok) {
            themes.append(loaded.theme);
        }
    }
    return themes;
}

QVariantMap snapshotWire(const QVariantMap &values)
{
    QVariantMap sources;
    for (const QString &key : AppearanceKeys::scopedKeys()) {
        sources.insert(key, QStringLiteral("user-overrides"));
    }
    return {{QLatin1StringView(WireContract::FieldStatus),
             quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion),
             quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch),
             QStringLiteral("font-catalog-epoch")},
            {QLatin1StringView(WireContract::FieldRevision), quint64(7)},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), sources},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}
} // namespace

class MonospaceCatalogTests final : public QObject {
    Q_OBJECT
private slots:
    void installedDefaultAllowsEditingButProportionalFamilyDoesNot();
    void missingSavedFamilyRemainsVisibleAndInvalid();
};

void MonospaceCatalogTests::installedDefaultAllowsEditingButProportionalFamilyDoesNot()
{
    if (!QFontDatabase::families().contains(QStringLiteral("Noto Sans Mono"))) {
        QSKIP("Shipped Noto Sans Mono is unavailable on this test host");
    }
    SnapshotTransport transport;
    SettingsClient client(transport, AppearanceKeys::scopedKeys());
    AppearanceSettingsModel model(client, fixtureThemes(), {}, Qt::ColorScheme::Light);
    const QStringList catalog = model.installedMonospaceFamilies();
    QVERIFY(catalog.contains(QStringLiteral("Noto Sans Mono")));
    if (QFontDatabase::families().contains(QStringLiteral("Noto Sans"))) {
        QVERIFY(!catalog.contains(QStringLiteral("Noto Sans")));
    }

    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.701"));
    QTRY_VERIFY(transport.snapshotToken != 0);
    Q_EMIT transport.snapshotReceived(transport.snapshotToken,
                                      transport.snapshotOwner,
                                      snapshotWire(AppearanceValues{}.toVariantMap()));
    QTRY_VERIFY(model.ready());
    QVERIFY2(model.draftValid(), qPrintable(model.fieldErrors().value(
        QLatin1String(AppearanceKeys::FontMonospaceFamily)).toString()));
    QVERIFY(model.setDraftValue(QLatin1String(AppearanceKeys::FontPointSize), 13.0));
    QVERIFY(model.applyAvailable());

    if (QFontDatabase::families().contains(QStringLiteral("Noto Sans"))) {
        QVERIFY(model.setDraftValue(QLatin1String(AppearanceKeys::FontMonospaceFamily),
                                    QStringLiteral("Noto Sans")));
        QVERIFY(!model.draftValid());
        QVERIFY(!model.applyAvailable());
        QVERIFY(model.fieldErrors().contains(
            QLatin1String(AppearanceKeys::FontMonospaceFamily)));
    }
    client.stop();
}

void MonospaceCatalogTests::missingSavedFamilyRemainsVisibleAndInvalid()
{
    SnapshotTransport transport;
    SettingsClient client(transport, AppearanceKeys::scopedKeys());
    AppearanceSettingsModel model(client, fixtureThemes(), {}, Qt::ColorScheme::Light);
    QVariantMap values = AppearanceValues{}.toVariantMap();
    const QString missing = QStringLiteral("Definitely Missing Monospace Family");
    QVERIFY(!model.installedMonospaceFamilies().contains(missing));
    values.insert(QLatin1String(AppearanceKeys::FontMonospaceFamily), missing);

    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.702"));
    QTRY_VERIFY(transport.snapshotToken != 0);
    Q_EMIT transport.snapshotReceived(transport.snapshotToken,
                                      transport.snapshotOwner, snapshotWire(values));
    QTRY_VERIFY(model.ready());
    QCOMPARE(model.confirmedMonospaceFamily(), missing);
    QCOMPARE(model.draft().value(QLatin1String(AppearanceKeys::FontMonospaceFamily))
                 .toString(), missing);
    QVERIFY(!model.draftValid());
    QVERIFY(model.fieldErrors().contains(
        QLatin1String(AppearanceKeys::FontMonospaceFamily)));
    client.stop();
}

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    MonospaceCatalogTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_appearance_monospace_catalog.moc"
