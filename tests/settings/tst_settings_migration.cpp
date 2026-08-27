// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/settings/layered_settings.h"
#include "qindaqt/settings/settings_migration.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <optional>

using namespace QindaQt::Settings;

class SettingsMigrationTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void migratesAValidV1DocumentLosslessly();
    void migratedDocumentDefaultsDoNotDisturbToFalse();
    void rejectsCorruptOrInvalidV1Input();
    void rejectsMismatchedSchemaObjects();
    void compatibilityLoaderReadsActiveVersionDirectly();
    void compatibilityLoaderMigratesALegacyFileWithoutMutatingIt();
    void compatibilityLoaderRejectsAnUnsupportedVersionWithoutMutating();
    void migrationIsIdempotentWhenAppliedTwice();

private:
    std::optional<SettingsSchema> m_v1Schema;
    std::optional<SettingsSchema> m_v2Schema;
};

void SettingsMigrationTests::initTestCase()
{
    QString error;
    m_v1Schema = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v1.json"), nullptr, &error, 1);
    QVERIFY2(m_v1Schema.has_value(), qPrintable(error));
    m_v2Schema = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"), nullptr, &error);
    QVERIFY2(m_v2Schema.has_value(), qPrintable(error));
}

void SettingsMigrationTests::migratesAValidV1DocumentLosslessly()
{
    constexpr auto v1Document = R"json({
      "schemaVersion": 1,
      "layer": "user-overrides",
      "values": {"appearance.theme": "qinda-light", "fonts.pointSize": 12.5}
    })json";
    const auto migrated =
        SettingsMigration::migrateV1ToV2(v1Document, QStringLiteral("fixture"), *m_v1Schema, *m_v2Schema);
    QVERIFY2(migrated.ok, qPrintable(migrated.error));
    QCOMPARE(migrated.sourceSchemaVersion, 1);
    QCOMPARE(migrated.document.schemaVersion, 2);
    QVERIFY(migrated.document.layer == SettingLayer::UserOverrides);
    QCOMPARE(migrated.document.values.value(QStringLiteral("appearance.theme")).toString(),
             QStringLiteral("qinda-light"));
    QCOMPARE(migrated.document.values.value(QStringLiteral("fonts.pointSize")).toDouble(), 12.5);
}

void SettingsMigrationTests::migratedDocumentDefaultsDoNotDisturbToFalse()
{
    constexpr auto v1Document = R"json({
      "schemaVersion": 1,
      "layer": "user-overrides",
      "values": {}
    })json";
    const auto migrated =
        SettingsMigration::migrateV1ToV2(v1Document, QStringLiteral("fixture"), *m_v1Schema, *m_v2Schema);
    QVERIFY2(migrated.ok, qPrintable(migrated.error));
    QVERIFY(!migrated.document.values.contains(QStringLiteral("services.doNotDisturb")));

    LayeredSettings settings(*m_v2Schema);
    const auto applied = settings.replaceLayer(migrated.document.layer, migrated.document.values);
    QVERIFY2(applied.ok(), qPrintable(applied.message));
    QCOMPARE(settings.value(QStringLiteral("services.doNotDisturb")).toBool(), false);
    QVERIFY(settings.sourceLayer(QStringLiteral("services.doNotDisturb"))
            == std::optional(SettingLayer::SystemDefaults));
}

void SettingsMigrationTests::rejectsCorruptOrInvalidV1Input()
{
    const auto corrupt =
        SettingsMigration::migrateV1ToV2("{not json", QStringLiteral("fixture"), *m_v1Schema, *m_v2Schema);
    QVERIFY(!corrupt.ok);

    constexpr auto invalidV1 = R"json({
      "schemaVersion": 1,
      "layer": "user-overrides",
      "values": {"windowManagement.snapDistance": 999, "unknown.key": true}
    })json";
    const auto invalid =
        SettingsMigration::migrateV1ToV2(invalidV1, QStringLiteral("fixture"), *m_v1Schema, *m_v2Schema);
    QVERIFY(!invalid.ok);
    QVERIFY(invalid.validation.issues().size() == 2);
}

void SettingsMigrationTests::rejectsMismatchedSchemaObjects()
{
    const auto swapped =
        SettingsMigration::migrateV1ToV2("{}", QStringLiteral("fixture"), *m_v2Schema, *m_v1Schema);
    QVERIFY(!swapped.ok);
    QVERIFY(swapped.error.contains(QStringLiteral("requires schema versions 1 and 2")));
}

void SettingsMigrationTests::compatibilityLoaderReadsActiveVersionDirectly()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    const SettingsDocument v2Document{.schemaVersion = 2,
                                      .layer = SettingLayer::UserOverrides,
                                      .values = {{QStringLiteral("services.doNotDisturb"), true}}};
    QString error;
    QVERIFY2(SettingsFileStore::save(path, v2Document, *m_v2Schema, nullptr, &error), qPrintable(error));

    const auto loaded = SettingsCompatibilityLoader::load(path, *m_v2Schema, *m_v1Schema);
    QVERIFY2(loaded.ok, qPrintable(loaded.error));
    QCOMPARE(loaded.sourceSchemaVersion, 2);
    QCOMPARE(loaded.document.schemaVersion, 2);
    QCOMPARE(loaded.document.values.value(QStringLiteral("services.doNotDisturb")).toBool(), true);
}

void SettingsMigrationTests::compatibilityLoaderMigratesALegacyFileWithoutMutatingIt()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    const SettingsDocument v1Document{.schemaVersion = 1,
                                      .layer = SettingLayer::UserOverrides,
                                      .values = {{QStringLiteral("appearance.theme"),
                                                  QStringLiteral("qinda-light")}}};
    QString error;
    QVERIFY2(SettingsFileStore::save(path, v1Document, *m_v1Schema, nullptr, &error), qPrintable(error));
    QFile beforeFile(path);
    QVERIFY(beforeFile.open(QIODevice::ReadOnly));
    const auto before = beforeFile.readAll();
    beforeFile.close();

    const auto loaded = SettingsCompatibilityLoader::load(path, *m_v2Schema, *m_v1Schema);
    QVERIFY2(loaded.ok, qPrintable(loaded.error));
    QCOMPARE(loaded.sourceSchemaVersion, 1);
    QCOMPARE(loaded.document.schemaVersion, 2);
    QCOMPARE(loaded.document.values.value(QStringLiteral("appearance.theme")).toString(),
             QStringLiteral("qinda-light"));
    QVERIFY(!loaded.document.values.contains(QStringLiteral("services.doNotDisturb")));

    // AGENT-CONTRACT proof: SettingsCompatibilityLoader::load() never writes.
    // The on-disk file is byte-for-byte unchanged; a caller decides whether
    // and how to persist the migrated result.
    QFile afterFile(path);
    QVERIFY(afterFile.open(QIODevice::ReadOnly));
    QCOMPARE(afterFile.readAll(), before);
}

void SettingsMigrationTests::compatibilityLoaderRejectsAnUnsupportedVersionWithoutMutating()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray original = R"json({"schemaVersion": 99, "layer": "user-overrides", "values": {}})json";
    QVERIFY(file.write(original) == original.size());
    file.close();

    const auto loaded = SettingsCompatibilityLoader::load(path, *m_v2Schema, *m_v1Schema);
    QVERIFY(!loaded.ok);
    QVERIFY(loaded.error.contains(QStringLiteral("unsupported schemaVersion 99")));

    QFile afterFile(path);
    QVERIFY(afterFile.open(QIODevice::ReadOnly));
    QCOMPARE(afterFile.readAll(), original);
}

void SettingsMigrationTests::migrationIsIdempotentWhenAppliedTwice()
{
    constexpr auto v1Document = R"json({
      "schemaVersion": 1,
      "layer": "user-overrides",
      "values": {"services.notifications": false}
    })json";
    const auto first =
        SettingsMigration::migrateV1ToV2(v1Document, QStringLiteral("fixture"), *m_v1Schema, *m_v2Schema);
    QVERIFY2(first.ok, qPrintable(first.error));

    // Re-migrating identical v1 input is deterministic: the same document
    // results. Re-reading an already-migrated v2 document through the
    // compatibility loader takes the direct (non-migrating) path and yields
    // the same values again.
    const auto second =
        SettingsMigration::migrateV1ToV2(v1Document, QStringLiteral("fixture"), *m_v1Schema, *m_v2Schema);
    QVERIFY2(second.ok, qPrintable(second.error));
    QCOMPARE(first.document.values, second.document.values);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    QString error;
    QVERIFY2(SettingsFileStore::save(path, first.document, *m_v2Schema, nullptr, &error), qPrintable(error));
    const auto reloaded = SettingsCompatibilityLoader::load(path, *m_v2Schema, *m_v1Schema);
    QVERIFY2(reloaded.ok, qPrintable(reloaded.error));
    QCOMPARE(reloaded.document.values, first.document.values);
}

QTEST_GUILESS_MAIN(SettingsMigrationTests)
#include "tst_settings_migration.moc"
