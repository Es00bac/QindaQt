// SPDX-License-Identifier: GPL-3.0-or-later
#include "catalogpaths.h"

#include <QDir>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Shell;

class CatalogPathsTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void explicitAndEnvironmentPathsAreIsolated();
    void mergedDirectoriesRunSourceThenSystemThenUser();
    void buildTreeSourceRequiresTheGenuineExecutable();

private:
    QTemporaryDir m_root;
    QByteArray m_priorProfileDir;
    QByteArray m_priorDataHome;
    QByteArray m_priorDataDirs;
};

void CatalogPathsTests::initTestCase()
{
    QVERIFY(m_root.isValid());
    m_priorProfileDir = qgetenv("QINDAQT_PROFILE_DIR");
    m_priorDataHome = qgetenv("XDG_DATA_HOME");
    m_priorDataDirs = qgetenv("XDG_DATA_DIRS");
}

void CatalogPathsTests::cleanup()
{
    qunsetenv("QINDAQT_PROFILE_DIR");
    if (!m_priorProfileDir.isEmpty()) {
        qputenv("QINDAQT_PROFILE_DIR", m_priorProfileDir);
    }
    if (!m_priorDataHome.isEmpty()) {
        qputenv("XDG_DATA_HOME", m_priorDataHome);
    }
    if (!m_priorDataDirs.isEmpty()) {
        qputenv("XDG_DATA_DIRS", m_priorDataDirs);
    }
}

void CatalogPathsTests::explicitAndEnvironmentPathsAreIsolated()
{
    const QString explicitPath = m_root.filePath(QStringLiteral("explicit"));
    QCOMPARE(resolveProfileCatalogDirectories(explicitPath, {}),
             QStringList{QDir::cleanPath(explicitPath)});

    const QString environmentPath = m_root.filePath(QStringLiteral("env"));
    qputenv("QINDAQT_PROFILE_DIR", environmentPath.toUtf8());
    QCOMPARE(resolveProfileCatalogDirectories({}, {}),
             QStringList{QDir::cleanPath(environmentPath)});
    qunsetenv("QINDAQT_PROFILE_DIR");
}

void CatalogPathsTests::mergedDirectoriesRunSourceThenSystemThenUser()
{
    const auto makeCatalog = [this](const QString &relative) {
        const QString path = m_root.filePath(relative + QStringLiteral("/qindaqt/profiles"));
        if (!QDir().mkpath(path)) {
            return QString{};
        }
        return QDir::cleanPath(path);
    };
    const QString userCatalog = makeCatalog(QStringLiteral("home"));
    const QString systemCatalog = makeCatalog(QStringLiteral("sys"));
    const QString sourceCatalog = makeCatalog(QStringLiteral("source"));
    QVERIFY(!userCatalog.isEmpty());
    QVERIFY(!systemCatalog.isEmpty());
    QVERIFY(!sourceCatalog.isEmpty());

    qputenv("XDG_DATA_HOME", m_root.filePath(QStringLiteral("home")).toUtf8());
    qputenv("XDG_DATA_DIRS", m_root.filePath(QStringLiteral("sys")).toUtf8());

    // Low-to-high precedence: source (build-tree development), installed
    // system roots, then the writable user store with the final say.
    QCOMPARE(resolveProfileCatalogDirectories({}, sourceCatalog),
             QStringList({sourceCatalog, systemCatalog, userCatalog}));

    // An absent source path (installed/relocated shell) contributes nothing.
    QCOMPARE(resolveProfileCatalogDirectories({}, {}),
             QStringList({systemCatalog, userCatalog}));
}

void CatalogPathsTests::buildTreeSourceRequiresTheGenuineExecutable()
{
    const QString source = m_root.filePath(QStringLiteral("genuine-source"));
    QVERIFY(QDir().mkpath(source));
    const QByteArray self = QCoreApplication::applicationFilePath().toUtf8();

    QCOMPARE(buildTreeSourceDirectory(source.toUtf8().constData(), self.constData()),
             source);
    // A different build executable (installed/relocated binary) must not
    // inherit the source-tree fallback.
    QVERIFY(buildTreeSourceDirectory(source.toUtf8().constData(),
                                     "/usr/bin/qindaqt-shell")
                .isEmpty());
    QVERIFY(buildTreeSourceDirectory("/definitely/absent", self.constData())
                .isEmpty());
    QVERIFY(buildTreeSourceDirectory(nullptr, self.constData()).isEmpty());
    QVERIFY(buildTreeSourceDirectory(source.toUtf8().constData(), nullptr).isEmpty());
}

QTEST_GUILESS_MAIN(CatalogPathsTests)
#include "tst_catalogpaths.moc"
