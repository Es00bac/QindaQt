// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/settings_service/settings_repository.h"
#include "qindaqt/settings/settings_document.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Services::SettingsService;
using namespace QindaQt::Settings;

class SettingsRepositoryTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void commitPersistsBeforeAuthoritativeSwap();
    void noOpConflictAndSaveFailureAreAtomic();
    void rejectsInvalidUnknownAndExhaustedInput();
    void unknownKeysPrecedeRevisionChecksAndHaveNoAuthority();
private:
    std::optional<SettingsSchema> m_schema;
};

void SettingsRepositoryTests::initTestCase()
{
    QString error;
    m_schema = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"), nullptr, &error);
    QVERIFY2(m_schema.has_value(), qPrintable(error));
}

void SettingsRepositoryTests::commitPersistsBeforeAuthoritativeSwap()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("user.json"));
    SettingsRepository repository(LayeredSettings(*m_schema), path, QStringLiteral("epoch"));
    const QVector<SettingsRepository::Operation> operations{
        {.key = QStringLiteral("services.doNotDisturb"), .remove = false, .value = true}};
    const auto result = repository.commitUserOverrides(0, operations);
    QVERIFY2(result.ok(), qPrintable(result.message));
    QCOMPARE(result.revisionAfter, quint64(1));
    QCOMPARE(repository.snapshot({QStringLiteral("services.doNotDisturb")}).values
                 .value(QStringLiteral("services.doNotDisturb")).toBool(), true);
    const auto disk = SettingsFileStore::load(path, *m_schema);
    QVERIFY2(disk.ok, qPrintable(disk.error));
    QCOMPARE(disk.document.values.value(QStringLiteral("services.doNotDisturb")).toBool(), true);
}

void SettingsRepositoryTests::noOpConflictAndSaveFailureAreAtomic()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("user.json"));
    SettingsRepository repository(LayeredSettings(*m_schema), path, QStringLiteral("epoch"));
    const QVector<SettingsRepository::Operation> setFalse{
        {.key = QStringLiteral("services.doNotDisturb"), .remove = false, .value = false}};
    const auto noOp = repository.commitUserOverrides(0, setFalse);
    QVERIFY(noOp.ok());
    QCOMPARE(noOp.revisionAfter, quint64(1));
    QVERIFY(QFile::exists(path));
    const auto same = repository.commitUserOverrides(1, setFalse);
    QVERIFY(same.ok());
    QCOMPARE(same.revisionAfter, quint64(1));
    QVERIFY(same.changedKeys.isEmpty());
    const auto conflict = repository.commitUserOverrides(0, setFalse);
    QVERIFY(conflict.status == RepositoryCommitStatus::Conflict);
    QCOMPARE(repository.revision(), quint64(1));

    const QString missingParent = directory.filePath(QStringLiteral("missing/user.json"));
    SettingsRepository failing(LayeredSettings(*m_schema), missingParent, QStringLiteral("epoch"));
    const QVector<SettingsRepository::Operation> setTrue{
        {.key = QStringLiteral("services.doNotDisturb"), .remove = false, .value = true}};
    const auto failed = failing.commitUserOverrides(0, setTrue);
    QVERIFY(failed.status == RepositoryCommitStatus::PersistenceFailed);
    QCOMPARE(failing.revision(), quint64(0));
    QCOMPARE(failing.snapshot({QStringLiteral("services.doNotDisturb")}).values
                 .value(QStringLiteral("services.doNotDisturb")).toBool(), false);
    QVERIFY(!QFile::exists(missingParent));
}

void SettingsRepositoryTests::rejectsInvalidUnknownAndExhaustedInput()
{
    QTemporaryDir directory;
    SettingsRepository repository(LayeredSettings(*m_schema),
                                  directory.filePath(QStringLiteral("user.json")),
                                  QStringLiteral("epoch"));
    const auto invalid = repository.commitUserOverrides(0, {{
        .key = QStringLiteral("services.doNotDisturb"), .remove = false,
        .value = QStringLiteral("yes")}});
    QVERIFY(invalid.status == RepositoryCommitStatus::ValidationFailed);
    const auto unknown = repository.commitUserOverrides(0, {{
        .key = QStringLiteral("unknown.key"), .remove = false, .value = true}});
    QVERIFY(unknown.status == RepositoryCommitStatus::UnknownKey);
    QVERIFY(unknown.currentValues.isEmpty());
    QVERIFY(unknown.currentSourceLayers.isEmpty());
    SettingsRepository exhausted(LayeredSettings(*m_schema),
                                 directory.filePath(QStringLiteral("exhausted.json")),
                                 QStringLiteral("epoch-exhausted"),
                                 std::numeric_limits<quint64>::max());
    const auto exhaustedResult = exhausted.commitUserOverrides(
        std::numeric_limits<quint64>::max(), {{
            .key = QStringLiteral("services.doNotDisturb"), .remove = false, .value = true}});
    QVERIFY(exhaustedResult.status == RepositoryCommitStatus::RevisionExhausted);
    QCOMPARE(exhaustedResult.revisionBefore, std::numeric_limits<quint64>::max());
    QCOMPARE(exhaustedResult.revisionAfter, std::numeric_limits<quint64>::max());
    QCOMPARE(exhausted.snapshot({QStringLiteral("services.doNotDisturb")}).values
                 .value(QStringLiteral("services.doNotDisturb")).toBool(), false);
    QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("exhausted.json"))));
}

void SettingsRepositoryTests::unknownKeysPrecedeRevisionChecksAndHaveNoAuthority()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("user.json"));
    SettingsRepository repository(LayeredSettings(*m_schema), path,
                                  QStringLiteral("epoch"), 7);
    for (const bool remove : {false, true}) {
        const auto result = repository.commitUserOverrides(
            2, {{.key = QStringLiteral("unknown.key"),
                 .remove = remove,
                 .value = remove ? QVariant{} : QVariant::fromValue(true)}});
        QVERIFY(result.status == RepositoryCommitStatus::UnknownKey);
        QCOMPARE(result.revisionBefore, quint64(7));
        QCOMPARE(result.revisionAfter, quint64(7));
        QVERIFY(result.currentValues.isEmpty());
        QVERIFY(result.currentSourceLayers.isEmpty());
        QVERIFY(result.changedKeys.isEmpty());
        QVERIFY(result.message.contains(QStringLiteral("unknown.key")));
    }
    QCOMPARE(repository.revision(), quint64(7));
    QVERIFY(!QFile::exists(path));
    QCOMPARE(repository.snapshot({QStringLiteral("services.doNotDisturb")})
                 .values.value(QStringLiteral("services.doNotDisturb")).toBool(), false);

    SettingsRepository exhausted(LayeredSettings(*m_schema),
                                 directory.filePath(QStringLiteral("exhausted-unknown.json")),
                                 QStringLiteral("epoch-exhausted"),
                                 std::numeric_limits<quint64>::max());
    const auto unknownAtLimit = exhausted.commitUserOverrides(
        0, {{.key = QStringLiteral("unknown.key"), .remove = true, .value = {}}});
    QVERIFY(unknownAtLimit.status == RepositoryCommitStatus::UnknownKey);
    QCOMPARE(unknownAtLimit.revisionBefore, std::numeric_limits<quint64>::max());
    QCOMPARE(unknownAtLimit.revisionAfter, std::numeric_limits<quint64>::max());
    QVERIFY(unknownAtLimit.currentValues.isEmpty());
    QVERIFY(unknownAtLimit.currentSourceLayers.isEmpty());
    QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("exhausted-unknown.json"))));
}

QTEST_GUILESS_MAIN(SettingsRepositoryTests)
#include "tst_settings_repository.moc"
