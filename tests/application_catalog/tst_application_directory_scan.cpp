// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/application_catalog/application_directory_scan.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::ApplicationCatalog;

namespace {

constexpr auto desktopTemplate = "[Desktop Entry]\n"
                                 "Type=Application\n"
                                 "Name=%1\n"
                                 "Exec=/usr/bin/%2\n"
                                 "%3";

bool writeFile(const QString &path, const QString &text)
{
    if (!QDir().mkpath(QFileInfo(path).path())) {
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    return file.write(text.toUtf8()) >= 0;
}

struct ScanFixture final
{
    QTemporaryDir firstRoot;
    QTemporaryDir secondRoot;

    [[nodiscard]] QStringList roots() const
    {
        return {firstRoot.path(), secondRoot.path()};
    }
};

} // namespace

class ApplicationDirectoryScanTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void scansEntriesWithRootPrecedenceAndIds();
    void skipsHiddenEntriesButRetainsValidatedDocuments();
    void rejectsEmptyRootLists();
};

void ApplicationDirectoryScanTests::scansEntriesWithRootPrecedenceAndIds()
{
    ScanFixture fixture;
    QVERIFY(writeFile(
        fixture.firstRoot.filePath("applications/first.desktop"),
        QString::fromUtf8(desktopTemplate).arg("First", "first", QString{})));
    QVERIFY(writeFile(
        fixture.secondRoot.filePath("applications/first.desktop"),
        QString::fromUtf8(desktopTemplate).arg("Second", "second", QString{})));
    // A nested file exercises the XDG id rule (separator becomes '-').
    QVERIFY(writeFile(
        fixture.secondRoot.filePath("applications/sub/nested.desktop"),
        QString::fromUtf8(desktopTemplate).arg("Nested", "nested", QString{})));

    QString error;
    const auto scan = scanApplicationDirectories(fixture.roots(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(scan.applications.size(), 2);
    // First data root wins the id contest.
    const auto *first = scan.application(QStringLiteral("first"));
    QVERIFY(first);
    QCOMPARE(first->entry.name, QStringLiteral("First"));
    QVERIFY(first->desktopFilePath.endsWith(QStringLiteral("first.desktop")));
    QVERIFY(first->documentText.contains(QStringLiteral("Exec=/usr/bin/first")));
    QVERIFY(scan.application(QStringLiteral("sub-nested")));
    // The second root's "first" duplicate is a real catalog diagnostic.
    QCOMPARE(scan.diagnostics.size(), 1);
    QCOMPARE(scan.diagnostics.constFirst().sourceId, QStringLiteral("first"));
}

void ApplicationDirectoryScanTests::skipsHiddenEntriesButRetainsValidatedDocuments()
{
    ScanFixture fixture;
    QVERIFY(writeFile(
        fixture.firstRoot.filePath("applications/visible.desktop"),
        QString::fromUtf8(desktopTemplate).arg("Visible", "visible", QString{})));
    QVERIFY(writeFile(
        fixture.firstRoot.filePath("applications/hidden.desktop"),
        QString::fromUtf8(desktopTemplate).arg(
            "Hidden", "hidden", QStringLiteral("NoDisplay=true\n"))));

    QString error;
    const auto scan = scanApplicationDirectories(fixture.roots(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(scan.applications.size(), 1);
    QCOMPARE(scan.applications.constFirst().entry.id,
             QStringLiteral("visible"));
    // Hidden documents are a normal producer hint, not a diagnostic.
    QVERIFY(scan.diagnostics.isEmpty());
}

void ApplicationDirectoryScanTests::rejectsEmptyRootLists()
{
    QString error;
    const auto scan = scanApplicationDirectories({}, &error);
    QVERIFY(scan.applications.isEmpty());
    QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(ApplicationDirectoryScanTests)
#include "tst_application_directory_scan.moc"
