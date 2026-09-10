// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime_layout_adoption.h"

#include "qindaqt/profiles/profile_catalog.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <algorithm>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using QindaQt::Profiles::ProfileCatalog;
using Outcome = QindaQt::Shell::RuntimeLayoutAdoption::Outcome;
namespace Adoption = QindaQt::Shell::RuntimeLayoutAdoption;

// The live-adoption catalog policy: reload from the remembered directories,
// honor the command-line lock, and fail closed to the prior selection. The
// user store participates last, exactly as in the shell's startup merge.
class RuntimeLayoutAdoptionTests final : public QObject {
    Q_OBJECT

private slots:
    void contentReloadKeepsSelectionAndAdoptsSavedPanels();
    void selectionAdoptionSwitchesProfile();
    void unknownSelectionFailsClosedToPriorLayout();
    void commandLineLockLeavesCatalogUntouched();
    void reloadFailureKeepsPriorCatalog();

private:
    [[nodiscard]] static bool copyFile(const QString &source,
                                       const QString &target)
    {
        return QFile::exists(source) && QFile::copy(source, target);
    }
};

namespace {

constexpr auto qindaqtSource =
    QINDAQT_SOURCE_DIR "/data/profiles/qindaqt.json";
constexpr auto minimalSource =
    QINDAQT_SOURCE_DIR "/data/profiles/minimal.json";

// Writes a user-store copy of the qindaqt profile keeping only its first
// panel, the way the Customize route persists an edited layout.
bool writeTrimmedUserProfile(const QString &target)
{
    QFile source(QString::fromLatin1(qindaqtSource));
    if (!source.open(QIODevice::ReadOnly)) {
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(source.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject()) {
        return false;
    }
    QJsonObject root = document.object();
    const auto panels = root.value(QStringLiteral("panels")).toArray();
    if (panels.isEmpty()) {
        return false;
    }
    root.insert(QStringLiteral("panels"), QJsonArray{panels.first()});
    QFile output(target);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return output.write(QJsonDocument(root).toJson()) > 0;
}

qsizetype panelCount(const ProfileCatalog &catalog, const QString &id)
{
    const auto &profiles = catalog.profiles();
    const auto found = std::find_if(
        profiles.cbegin(), profiles.cend(),
        [&id](const auto &profile) { return profile.id == id; });
    return found == profiles.cend() ? -1 : found->panels.size();
}

} // namespace

void RuntimeLayoutAdoptionTests::
    contentReloadKeepsSelectionAndAdoptsSavedPanels()
{
    QTemporaryDir builtin;
    QTemporaryDir user;
    QVERIFY(builtin.isValid() && user.isValid());
    QVERIFY(copyFile(QString::fromLatin1(qindaqtSource),
                     builtin.filePath(QStringLiteral("qindaqt.json"))));
    QVERIFY(copyFile(QString::fromLatin1(minimalSource),
                     builtin.filePath(QStringLiteral("minimal.json"))));
    const QStringList directories{builtin.path(), user.path()};

    ProfileCatalog catalog;
    QString error;
    QVERIFY(catalog.loadDirectories(directories, &error));
    QVERIFY(catalog.selectById(QStringLiteral("qindaqt")));
    const qsizetype builtinPanelCount =
        panelCount(catalog, QStringLiteral("qindaqt"));
    QVERIFY(builtinPanelCount > 1);

    QVERIFY(writeTrimmedUserProfile(
        user.filePath(QStringLiteral("qindaqt.json"))));

    QString diagnostic;
    QCOMPARE(Adoption::reloadAndSelect(catalog, directories, QString(), false,
                                        &diagnostic),
             Outcome::AdoptedContent);
    QCOMPARE(catalog.current().value(QStringLiteral("id")).toString(),
             QStringLiteral("qindaqt"));
    QCOMPARE(panelCount(catalog, QStringLiteral("qindaqt")), 1);
}

void RuntimeLayoutAdoptionTests::selectionAdoptionSwitchesProfile()
{
    QTemporaryDir builtin;
    QTemporaryDir user;
    QVERIFY(builtin.isValid() && user.isValid());
    QVERIFY(copyFile(QString::fromLatin1(qindaqtSource),
                     builtin.filePath(QStringLiteral("qindaqt.json"))));
    QVERIFY(copyFile(QString::fromLatin1(minimalSource),
                     builtin.filePath(QStringLiteral("minimal.json"))));
    const QStringList directories{builtin.path(), user.path()};

    ProfileCatalog catalog;
    QString error;
    QVERIFY(catalog.loadDirectories(directories, &error));
    QVERIFY(catalog.selectById(QStringLiteral("qindaqt")));

    QString diagnostic;
    QCOMPARE(Adoption::reloadAndSelect(
                 catalog, directories, QStringLiteral("minimal"), false,
                 &diagnostic),
             Outcome::AdoptedSelection);
    QCOMPARE(catalog.current().value(QStringLiteral("id")).toString(),
             QStringLiteral("minimal"));
}

void RuntimeLayoutAdoptionTests::unknownSelectionFailsClosedToPriorLayout()
{
    QTemporaryDir builtin;
    QTemporaryDir user;
    QVERIFY(builtin.isValid() && user.isValid());
    QVERIFY(copyFile(QString::fromLatin1(qindaqtSource),
                     builtin.filePath(QStringLiteral("qindaqt.json"))));
    QVERIFY(copyFile(QString::fromLatin1(minimalSource),
                     builtin.filePath(QStringLiteral("minimal.json"))));
    const QStringList directories{builtin.path(), user.path()};

    ProfileCatalog catalog;
    QString error;
    QVERIFY(catalog.loadDirectories(directories, &error));
    QVERIFY(catalog.selectById(QStringLiteral("qindaqt")));
    const qsizetype priorPanelCount =
        panelCount(catalog, QStringLiteral("qindaqt"));

    QString diagnostic;
    QCOMPARE(Adoption::reloadAndSelect(
                 catalog, directories, QStringLiteral("deleted-profile"),
                 false, &diagnostic),
             Outcome::KeptPriorSelection);
    QVERIFY(!diagnostic.isEmpty());
    QCOMPARE(catalog.current().value(QStringLiteral("id")).toString(),
             QStringLiteral("qindaqt"));
    QCOMPARE(panelCount(catalog, QStringLiteral("qindaqt")), priorPanelCount);
}

void RuntimeLayoutAdoptionTests::commandLineLockLeavesCatalogUntouched()
{
    QTemporaryDir builtin;
    QTemporaryDir user;
    QVERIFY(builtin.isValid() && user.isValid());
    QVERIFY(copyFile(QString::fromLatin1(qindaqtSource),
                     builtin.filePath(QStringLiteral("qindaqt.json"))));
    const QStringList directories{builtin.path(), user.path()};

    ProfileCatalog catalog;
    QString error;
    QVERIFY(catalog.loadDirectories(directories, &error));
    QVERIFY(catalog.selectById(QStringLiteral("qindaqt")));
    const qsizetype lockedPanelCount =
        panelCount(catalog, QStringLiteral("qindaqt"));

    QVERIFY(writeTrimmedUserProfile(
        user.filePath(QStringLiteral("qindaqt.json"))));

    QString diagnostic;
    QCOMPARE(Adoption::reloadAndSelect(
                 catalog, directories, QStringLiteral("minimal"), true,
                 &diagnostic),
             Outcome::LockedByCommandLine);
    QVERIFY(!diagnostic.isEmpty());
    // The lock skips the reload entirely: the unsaved user copy must not
    // leak into a locked session's catalog.
    QCOMPARE(panelCount(catalog, QStringLiteral("qindaqt")), lockedPanelCount);
    QCOMPARE(catalog.current().value(QStringLiteral("id")).toString(),
             QStringLiteral("qindaqt"));
}

void RuntimeLayoutAdoptionTests::reloadFailureKeepsPriorCatalog()
{
    QTemporaryDir builtin;
    QTemporaryDir broken;
    QVERIFY(builtin.isValid() && broken.isValid());
    QVERIFY(copyFile(QString::fromLatin1(qindaqtSource),
                     builtin.filePath(QStringLiteral("qindaqt.json"))));
    QFile invalid(broken.filePath(QStringLiteral("corrupt.json")));
    QVERIFY(invalid.open(QIODevice::WriteOnly));
    QVERIFY(invalid.write("{ not json") > 0);
    invalid.close();

    ProfileCatalog catalog;
    QString error;
    QVERIFY(catalog.loadDirectories({builtin.path()}, &error));
    QVERIFY(catalog.selectById(QStringLiteral("qindaqt")));
    const qsizetype failedPanelCount =
        panelCount(catalog, QStringLiteral("qindaqt"));

    QString diagnostic;
    QCOMPARE(Adoption::reloadAndSelect(
                 catalog, {builtin.path(), broken.path()}, QString(), false,
                 &diagnostic),
             Outcome::Failed);
    QVERIFY(!diagnostic.isEmpty());
    QCOMPARE(catalog.current().value(QStringLiteral("id")).toString(),
             QStringLiteral("qindaqt"));
    QCOMPARE(panelCount(catalog, QStringLiteral("qindaqt")), failedPanelCount);
}

QTEST_GUILESS_MAIN(RuntimeLayoutAdoptionTests)
#include "tst_runtimelayoutadoption.moc"
