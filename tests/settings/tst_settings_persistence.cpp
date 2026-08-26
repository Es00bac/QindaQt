// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/settings/layered_settings.h"
#include "qindaqt/settings/settings_document.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <optional>

using namespace QindaQt::Settings;

class SettingsPersistenceTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void loadsBuiltInProfileDefaults();
    void roundTripsAUserDocument();
    void rejectsVolatileAndInvalidDocuments();
    void failedSavePreservesExistingFile();

private:
    std::optional<SettingsSchema> m_schema;
};

void SettingsPersistenceTests::initTestCase()
{
    QString error;
    m_schema = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v1.json"), nullptr, &error);
    QVERIFY2(m_schema.has_value(), qPrintable(error));
}

void SettingsPersistenceTests::loadsBuiltInProfileDefaults()
{
    const auto loaded = SettingsFileStore::load(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/profile-defaults/qindaqt.json"), *m_schema);
    QVERIFY2(loaded.ok, qPrintable(loaded.error));
    QVERIFY(loaded.document.layer == SettingLayer::ProfileDefaults);

    LayeredSettings settings(*m_schema);
    const auto applied = settings.replaceLayer(loaded.document.layer, loaded.document.values);
    QVERIFY2(applied.ok(), qPrintable(applied.message));
    QCOMPARE(settings.value(QStringLiteral("appearance.animationDurationMs")).toLongLong(), 160);
}

void SettingsPersistenceTests::roundTripsAUserDocument()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    const SettingsDocument document{.schemaVersion = 1,
                                    .layer = SettingLayer::UserOverrides,
                                    .values = {{QStringLiteral("appearance.theme"),
                                                QStringLiteral("qinda-light")},
                                               {QStringLiteral("fonts.pointSize"), 11.5}}};

    QString error;
    QVERIFY2(SettingsFileStore::save(path, document, *m_schema, nullptr, &error), qPrintable(error));
    const auto loaded = SettingsFileStore::load(path, *m_schema);
    QVERIFY2(loaded.ok, qPrintable(loaded.error));
    QVERIFY(loaded.document.layer == SettingLayer::UserOverrides);
    QCOMPARE(loaded.document.values.value(QStringLiteral("appearance.theme")).toString(),
             QStringLiteral("qinda-light"));
    QCOMPARE(loaded.document.values.value(QStringLiteral("fonts.pointSize")).toDouble(), 11.5);
}

void SettingsPersistenceTests::rejectsVolatileAndInvalidDocuments()
{
    const SettingsDocument session{.schemaVersion = 1,
                                   .layer = SettingLayer::SessionOverrides,
                                   .values = {}};
    QString error;
    QVERIFY(!SettingsDocumentCodec::toJson(session, *m_schema, nullptr, &error).has_value());
    QVERIFY(error.contains(QStringLiteral("not persistable")));

    constexpr auto invalid = R"json({
      "schemaVersion": 1,
      "layer": "user-overrides",
      "values": {"windowManagement.snapDistance": 999, "unknown.key": true}
    })json";
    const auto loaded = SettingsDocumentCodec::fromJson(invalid, QStringLiteral("fixture"), *m_schema);
    QVERIFY(!loaded.ok);
    QVERIFY(loaded.validation.issues().size() == 2);
}

void SettingsPersistenceTests::failedSavePreservesExistingFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    const SettingsDocument valid{.schemaVersion = 1,
                                 .layer = SettingLayer::UserOverrides,
                                 .values = {{QStringLiteral("appearance.blurEnabled"), false}}};
    QString error;
    QVERIFY2(SettingsFileStore::save(path, valid, *m_schema, nullptr, &error), qPrintable(error));
    QFile beforeFile(path);
    QVERIFY(beforeFile.open(QIODevice::ReadOnly));
    const auto before = beforeFile.readAll();
    beforeFile.close();

    const SettingsDocument invalid{
        .schemaVersion = 1,
        .layer = SettingLayer::UserOverrides,
        .values = {{QStringLiteral("windowManagement.snapDistance"), 999}}};
    QVERIFY(!SettingsFileStore::save(path, invalid, *m_schema, nullptr, &error));
    QFile afterFile(path);
    QVERIFY(afterFile.open(QIODevice::ReadOnly));
    QCOMPARE(afterFile.readAll(), before);
}

QTEST_GUILESS_MAIN(SettingsPersistenceTests)
#include "tst_settings_persistence.moc"
