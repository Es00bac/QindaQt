// SPDX-License-Identifier: GPL-3.0-or-later
// The one-time import of KDE KWin settings into qindaqt-kwin's own files
// (ADR-0291): import once, never overwrite, never touch the KDE files.
#include "compositorconfigimport.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

using QindaQt::Session::CompositorConfigImport;

namespace {
void writeFile(const QString &path, const QByteArray &contents,
               QFileDevice::Permissions permissions = QFileDevice::ReadOwner | QFileDevice::WriteOwner)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(contents);
    file.close();
    QVERIFY(QFile::setPermissions(path, permissions));
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("<missing>");
}
} // namespace

class CompositorConfigImportTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void namesTheForkFilesInQindaQtFolders();
    void importsEveryMissingFile();
    void neverOverwritesAQindaQtFile();
    void importsOnlyOnce();
    void skipsAbsentKdeFiles();
    void keepsTheKdeFilesUntouched();
    void keepsPrivatePermissions();
    void reportsAnUnreadableKdeFile();
};

void CompositorConfigImportTest::namesTheForkFilesInQindaQtFolders()
{
    const auto pairs = CompositorConfigImport::pairs();
    QCOMPARE(pairs.size(), 6);
    QStringList targets;
    for (const auto &pair : pairs) {
        QVERIFY2(pair.qindaqtFile.startsWith(QStringLiteral("qindaqt/")), qPrintable(pair.qindaqtFile));
        QVERIFY(!pair.kdeFile.contains(QLatin1Char('/')));
        targets.append(pair.qindaqtFile);
    }
    QCOMPARE(targets, (QStringList{QStringLiteral("qindaqt/kwinrc"),
                                   QStringLiteral("qindaqt/kwinrulesrc"),
                                   QStringLiteral("qindaqt/kwinoutputconfig.json"),
                                   QStringLiteral("qindaqt/kwininputrc"),
                                   QStringLiteral("qindaqt/kwinxkbrc"),
                                   QStringLiteral("qindaqt/kwinstaterc")}));
}

void CompositorConfigImportTest::importsEveryMissingFile()
{
    QTemporaryDir config, state;
    QVERIFY(config.isValid() && state.isValid());
    for (const auto &pair : CompositorConfigImport::pairs()) {
        writeFile(QDir(pair.state ? state.path() : config.path()).filePath(pair.kdeFile),
                  "[Group]\nfrom=" + pair.kdeFile.toUtf8() + "\n");
    }
    QStringList imported;
    QString error;
    QVERIFY2(CompositorConfigImport::run(config.path(), state.path(), &imported, &error), qPrintable(error));
    QCOMPARE(imported.size(), 6);
    for (const auto &pair : CompositorConfigImport::pairs()) {
        const QDir home(pair.state ? state.path() : config.path());
        QCOMPARE(readFile(home.filePath(pair.qindaqtFile)), readFile(home.filePath(pair.kdeFile)));
    }
}

void CompositorConfigImportTest::neverOverwritesAQindaQtFile()
{
    QTemporaryDir config, state;
    QVERIFY(config.isValid() && state.isValid());
    writeFile(QDir(config.path()).filePath(QStringLiteral("kwinrc")), "[Windows]\nkde=1\n");
    writeFile(QDir(config.path()).filePath(QStringLiteral("qindaqt/kwinrc")), "[Windows]\nqindaqt=1\n");
    writeFile(QDir(config.path()).filePath(QStringLiteral("kxkbrc")), "[Layout]\nLayoutList=us\n");
    QStringList imported;
    QString error;
    QVERIFY2(CompositorConfigImport::run(config.path(), state.path(), &imported, &error), qPrintable(error));
    QCOMPARE(imported, QStringList{QStringLiteral("qindaqt/kwinxkbrc")});
    QCOMPARE(readFile(QDir(config.path()).filePath(QStringLiteral("qindaqt/kwinrc"))), QByteArray("[Windows]\nqindaqt=1\n"));
}

void CompositorConfigImportTest::importsOnlyOnce()
{
    QTemporaryDir config, state;
    QVERIFY(config.isValid() && state.isValid());
    const QString kde = QDir(config.path()).filePath(QStringLiteral("kcminputrc"));
    writeFile(kde, "[Mouse]\ncursorSize=24\n");
    QString error;
    QVERIFY2(CompositorConfigImport::run(config.path(), state.path(), nullptr, &error), qPrintable(error));
    writeFile(kde, "[Mouse]\ncursorSize=48\n");
    QStringList imported;
    QVERIFY2(CompositorConfigImport::run(config.path(), state.path(), &imported, &error), qPrintable(error));
    QVERIFY(imported.isEmpty());
    QCOMPARE(readFile(QDir(config.path()).filePath(QStringLiteral("qindaqt/kwininputrc"))), QByteArray("[Mouse]\ncursorSize=24\n"));
}

void CompositorConfigImportTest::skipsAbsentKdeFiles()
{
    QTemporaryDir config, state;
    QVERIFY(config.isValid() && state.isValid());
    QStringList imported;
    QString error;
    QVERIFY2(CompositorConfigImport::run(config.path(), state.path(), &imported, &error), qPrintable(error));
    QVERIFY(imported.isEmpty());
    QVERIFY(!QFileInfo::exists(QDir(config.path()).filePath(QStringLiteral("qindaqt"))));
    QVERIFY(!QFileInfo::exists(QDir(state.path()).filePath(QStringLiteral("qindaqt"))));
}

void CompositorConfigImportTest::keepsTheKdeFilesUntouched()
{
    QTemporaryDir config, state;
    QVERIFY(config.isValid() && state.isValid());
    const QString kde = QDir(state.path()).filePath(QStringLiteral("kwinstaterc"));
    writeFile(kde, "[Activities]\nlast=1\n");
    const QDateTime modified = QFileInfo(kde).lastModified();
    QString error;
    QVERIFY2(CompositorConfigImport::run(config.path(), state.path(), nullptr, &error), qPrintable(error));
    QCOMPARE(readFile(kde), QByteArray("[Activities]\nlast=1\n"));
    QCOMPARE(QFileInfo(kde).lastModified(), modified);
    QCOMPARE(readFile(QDir(state.path()).filePath(QStringLiteral("qindaqt/kwinstaterc"))), QByteArray("[Activities]\nlast=1\n"));
}

void CompositorConfigImportTest::keepsPrivatePermissions()
{
    QTemporaryDir config, state;
    QVERIFY(config.isValid() && state.isValid());
    writeFile(QDir(config.path()).filePath(QStringLiteral("kwinrulesrc")), "[1]\nDescription=private\n",
              QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QString error;
    QVERIFY2(CompositorConfigImport::run(config.path(), state.path(), nullptr, &error), qPrintable(error));
    const auto permissions = QFileInfo(QDir(config.path()).filePath(QStringLiteral("qindaqt/kwinrulesrc"))).permissions();
    QVERIFY(permissions.testFlag(QFileDevice::ReadOwner));
    QVERIFY(!permissions.testFlag(QFileDevice::ReadGroup));
    QVERIFY(!permissions.testFlag(QFileDevice::ReadOther));
}

void CompositorConfigImportTest::reportsAnUnreadableKdeFile()
{
    if (::geteuid() == 0) {
        QSKIP("root reads a file without read permission");
    }
    QTemporaryDir config, state;
    QVERIFY(config.isValid() && state.isValid());
    writeFile(QDir(config.path()).filePath(QStringLiteral("kwinrc")), "[Windows]\n", QFileDevice::WriteOwner);
    QString error;
    QVERIFY(!CompositorConfigImport::run(config.path(), state.path(), nullptr, &error));
    QVERIFY2(error.contains(QStringLiteral("kwinrc")), qPrintable(error));
    QVERIFY(!QFileInfo::exists(QDir(config.path()).filePath(QStringLiteral("qindaqt/kwinrc"))));
}

QTEST_GUILESS_MAIN(CompositorConfigImportTest)
#include "tst_compositorconfigimport.moc"
