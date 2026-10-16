// SPDX-License-Identifier: GPL-3.0-or-later
#include "catalogpaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
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
    void catalogDirectoriesMergeLowToHighWithTheUserLast();
    void catalogDirectoriesOmitMissingAndDuplicateLocations();
    void aStaleUserDirectoryNeverReplacesTheInstalledOne();
    void catalogFilesMergeLowToHighToo();
    void explicitAndEnvironmentOverridesStayIsolatedForMergedCatalogs();

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

namespace {

QString makeDirectory(const QTemporaryDir &root, const QString &relative)
{
    const QString path = root.filePath(relative);
    return QDir().mkpath(path) ? QDir::cleanPath(path) : QString{};
}

bool touchFile(const QTemporaryDir &root, const QString &relative)
{
    const QString path = root.filePath(relative);
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write("{}") == 2;
}

} // namespace

// AGENT-CONTRACT (ADR-0393): themes, applet manifests and the applet policy
// merge every XDG data directory, low to high precedence. QStandardPaths::
// locateAll answers high to low, so the resolver reverses it.
void CatalogPathsTests::catalogDirectoriesMergeLowToHighWithTheUserLast()
{
    const QTemporaryDir root; // isolated from the shared fixture root
    QVERIFY(root.isValid());
    const QString source = makeDirectory(root, QStringLiteral("source/themes"));
    const QString user = makeDirectory(root, QStringLiteral("home/qindaqt/themes"));
    const QString systemA = makeDirectory(root, QStringLiteral("sysA/qindaqt/themes"));
    const QString systemB = makeDirectory(root, QStringLiteral("sysB/qindaqt/themes"));
    QVERIFY(!source.isEmpty() && !user.isEmpty() && !systemA.isEmpty() && !systemB.isEmpty());
    qputenv("XDG_DATA_HOME", root.filePath(QStringLiteral("home")).toUtf8());
    // XDG_DATA_DIRS is high to low: sysA outranks sysB.
    qputenv("XDG_DATA_DIRS", (root.filePath(QStringLiteral("sysA")) + QLatin1Char(':')
                              + root.filePath(QStringLiteral("sysB"))).toUtf8());

    QCOMPARE(resolveCatalogDataDirectories({}, "QINDAQT_TEST_CATALOG_DIR",
                                           source.toUtf8().constData(),
                                           QStringLiteral("qindaqt/themes")),
             QStringList({source, systemB, systemA, user}));
    // An installed shell has no source tree: the system roots, then the user.
    QCOMPARE(resolveCatalogDataDirectories({}, "QINDAQT_TEST_CATALOG_DIR", nullptr,
                                           QStringLiteral("qindaqt/themes")),
             QStringList({systemB, systemA, user}));
    QCOMPARE(resolveCatalogDataDirectories({}, "QINDAQT_TEST_CATALOG_DIR", "/definitely/absent",
                                           QStringLiteral("qindaqt/themes")),
             QStringList({systemB, systemA, user}));
}

void CatalogPathsTests::catalogDirectoriesOmitMissingAndDuplicateLocations()
{
    const QTemporaryDir root; // isolated from the shared fixture root
    QVERIFY(root.isValid());
    const QString user = makeDirectory(root, QStringLiteral("home/qindaqt/applets"));
    const QString system = makeDirectory(root, QStringLiteral("sys/qindaqt/applets"));
    QVERIFY(!user.isEmpty() && !system.isEmpty());
    QVERIFY(QDir().mkpath(root.filePath(QStringLiteral("sys-without-applets"))));
    qputenv("XDG_DATA_HOME", root.filePath(QStringLiteral("home")).toUtf8());
    // A directory without the catalog, a repeat of the same root, and the user
    // home listed again among the system roots all collapse to one entry each.
    qputenv("XDG_DATA_DIRS",
            (root.filePath(QStringLiteral("sys-without-applets")) + QLatin1Char(':')
             + root.filePath(QStringLiteral("sys")) + QLatin1Char(':')
             + root.filePath(QStringLiteral("sys")) + QLatin1Char(':')
             + root.filePath(QStringLiteral("home"))).toUtf8());
    const QStringList resolved = resolveCatalogDataDirectories(
        {}, "QINDAQT_TEST_CATALOG_DIR", nullptr, QStringLiteral("qindaqt/applets"));
    QCOMPARE(resolved.count(system), 1);
    QCOMPARE(resolved.count(user), 1);
    QCOMPARE(resolved.size(), 2);

    qputenv("XDG_DATA_HOME", root.filePath(QStringLiteral("nowhere")).toUtf8());
    qputenv("XDG_DATA_DIRS", root.filePath(QStringLiteral("nowhere-either")).toUtf8());
    QVERIFY(resolveCatalogDataDirectories({}, "QINDAQT_TEST_CATALOG_DIR", nullptr,
                                          QStringLiteral("qindaqt/applets"))
                .isEmpty());
}

void CatalogPathsTests::aStaleUserDirectoryNeverReplacesTheInstalledOne()
{
    const QTemporaryDir root; // isolated from the shared fixture root
    QVERIFY(root.isValid());
    // The incident: ~/.local/share/qindaqt/themes existed, so locate() answered
    // with it alone and /usr/share/qindaqt/themes was never read.
    const QString stale = makeDirectory(root, QStringLiteral("home/qindaqt/themes"));
    const QString installed = makeDirectory(root, QStringLiteral("usr/share/qindaqt/themes"));
    qputenv("XDG_DATA_HOME", root.filePath(QStringLiteral("home")).toUtf8());
    qputenv("XDG_DATA_DIRS", root.filePath(QStringLiteral("usr/share")).toUtf8());

    const QString legacy = resolveCatalogDataDirectory(
        {}, "QINDAQT_TEST_CATALOG_DIR", "/definitely/absent", QStringLiteral("qindaqt/themes"));
    QCOMPARE(legacy, stale); // the first-match answer that hid the installed set
    const QStringList merged = resolveCatalogDataDirectories(
        {}, "QINDAQT_TEST_CATALOG_DIR", "/definitely/absent", QStringLiteral("qindaqt/themes"));
    QVERIFY(merged.contains(installed));
    QCOMPARE(merged, QStringList({installed, stale}));
}

void CatalogPathsTests::catalogFilesMergeLowToHighToo()
{
    const QTemporaryDir root; // isolated from the shared fixture root
    QVERIFY(root.isValid());
    const QString relative = QStringLiteral("qindaqt/applet-policy/default.json");
    QVERIFY(touchFile(root, QStringLiteral("home/") + relative));
    QVERIFY(touchFile(root, QStringLiteral("sysA/") + relative));
    QVERIFY(touchFile(root, QStringLiteral("sysB/") + relative));
    QVERIFY(touchFile(root, QStringLiteral("source/default.json")));
    qputenv("XDG_DATA_HOME", root.filePath(QStringLiteral("home")).toUtf8());
    qputenv("XDG_DATA_DIRS", (root.filePath(QStringLiteral("sysA")) + QLatin1Char(':')
                              + root.filePath(QStringLiteral("sysB"))).toUtf8());
    const auto path = [&root](const QString &tail) {
        return QDir::cleanPath(root.filePath(tail));
    };
    const QString source = root.filePath(QStringLiteral("source/default.json"));
    QCOMPARE(resolveCatalogDataFiles({}, "QINDAQT_TEST_CATALOG_FILE", source.toUtf8().constData(),
                                     relative),
             QStringList({path(QStringLiteral("source/default.json")),
                          path(QStringLiteral("sysB/") + relative),
                          path(QStringLiteral("sysA/") + relative),
                          path(QStringLiteral("home/") + relative)}));
    QCOMPARE(resolveCatalogDataFiles({}, "QINDAQT_TEST_CATALOG_FILE", "/definitely/absent.json",
                                     relative),
             QStringList({path(QStringLiteral("sysB/") + relative),
                          path(QStringLiteral("sysA/") + relative),
                          path(QStringLiteral("home/") + relative)}));
}

void CatalogPathsTests::explicitAndEnvironmentOverridesStayIsolatedForMergedCatalogs()
{
    const QTemporaryDir root; // isolated from the shared fixture root
    QVERIFY(root.isValid());
    const QString user = makeDirectory(root, QStringLiteral("home/qindaqt/themes"));
    QVERIFY(!user.isEmpty());
    qputenv("XDG_DATA_HOME", root.filePath(QStringLiteral("home")).toUtf8());
    qputenv("XDG_DATA_DIRS", root.filePath(QStringLiteral("home")).toUtf8());

    const QString explicitPath = root.filePath(QStringLiteral("explicit/../explicit"));
    QCOMPARE(resolveCatalogDataDirectories(explicitPath, "QINDAQT_TEST_CATALOG_DIR", nullptr,
                                           QStringLiteral("qindaqt/themes")),
             QStringList{QDir::cleanPath(explicitPath)});
    qputenv("QINDAQT_TEST_CATALOG_DIR", root.filePath(QStringLiteral("env")).toUtf8());
    QCOMPARE(resolveCatalogDataDirectories({}, "QINDAQT_TEST_CATALOG_DIR", nullptr,
                                           QStringLiteral("qindaqt/themes")),
             QStringList{QDir::cleanPath(root.filePath(QStringLiteral("env")))});
    QCOMPARE(resolveCatalogDataFiles({}, "QINDAQT_TEST_CATALOG_DIR", nullptr,
                                     QStringLiteral("qindaqt/applet-policy/default.json")),
             QStringList{QDir::cleanPath(root.filePath(QStringLiteral("env")))});
    qunsetenv("QINDAQT_TEST_CATALOG_DIR");
}

QTEST_GUILESS_MAIN(CatalogPathsTests)
#include "tst_catalogpaths.moc"
