// SPDX-License-Identifier: GPL-3.0-or-later
#include "managed_catalog_fixtures.h"
#include "qindaqt/application_catalog/application_directory_scan.h"
#include "qindaqt/application_catalog/category_tree.h"
#include "qindaqt/shell_launcher/launcher_bounds.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::ApplicationCatalog;
using namespace ManagedCatalogFixtures;

namespace {
bool writeDesktop(const QString &root, const QString &id, const QString &body)
{
    if (!QDir().mkpath(root + QStringLiteral("/applications")))
        return false;
    QFile file(root + QStringLiteral("/applications/") + id + QStringLiteral(".desktop"));
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const auto bytes = body.toUtf8();
    return file.write(bytes) == bytes.size();
}
}

class ManagedApplicationScanTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void nativeAndManagedRowsShareOneApplicationsTree();
    void overrideAndDeletionWithdrawBinding();
    void malformedOverrideCannotResurrectManagedEntry();
    void indeterminateScanWithholdsManagedRows();
    void invalidManagedSnapshotKeepsNativeRows();
};

void ManagedApplicationScanTest::nativeAndManagedRowsShareOneApplicationsTree()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeDesktop(root.path(), QStringLiteral("native.fixture"), desktop()));
    const auto value = registration();
    const auto scan = scanApplicationDirectories({root.path()}, {value});
    QVERIFY(scan.diagnostics.isEmpty());
    QCOMPARE(scan.applications.size(), 2);
    const auto row = scan.application(managedApplicationEntryId(value.binding));
    QVERIFY(row);
    QCOMPARE(row->entry.managedBinding, std::optional(value.binding));
    QVERIFY(row->documentText.isEmpty());
    QVERIFY(row->desktopFilePath.isEmpty());
    QVERIFY(!buildCategoryTree({row->entry}).isEmpty());
}

void ManagedApplicationScanTest::overrideAndDeletionWithdrawBinding()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto value = registration();
    const auto id = managedApplicationEntryId(value.binding);
    QVERIFY(writeDesktop(root.path(), id, desktop()));
    auto scan = scanApplicationDirectories({root.path()}, {value});
    QVERIFY(scan.application(id));
    QVERIFY(!scan.application(id)->entry.managedBinding);
    QVERIFY(writeDesktop(root.path(), id, desktop(QStringLiteral("Hidden=true\n"))));
    scan = scanApplicationDirectories({root.path()}, {value});
    QVERIFY(!scan.application(id));
}

void ManagedApplicationScanTest::malformedOverrideCannotResurrectManagedEntry()
{
    QTemporaryDir root;
    const auto value = registration();
    const auto id = managedApplicationEntryId(value.binding);
    QVERIFY(writeDesktop(root.path(), id, QStringLiteral("invalid document")));
    const auto scan = scanApplicationDirectories({root.path()}, {value});
    QVERIFY(!scan.application(id));
    QVERIFY(!scan.diagnostics.isEmpty());
}

void ManagedApplicationScanTest::indeterminateScanWithholdsManagedRows()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QFile notDirectory(root.path() + QStringLiteral("/not-a-data-root"));
    QVERIFY(notDirectory.open(QIODevice::WriteOnly));
    notDirectory.close();
    QVERIFY(writeDesktop(root.path(), QStringLiteral("native.fixture"), desktop()));
    const auto scan = scanApplicationDirectories(
        {notDirectory.fileName(), root.path()}, {registration()});
    QCOMPARE(scan.applications.size(), 1);
    QVERIFY(!scan.applications.first().entry.managedBinding);
    QVERIFY(!scan.diagnostics.isEmpty());
    QVERIFY(scan.diagnostics.size() <= Bounds::maxDiagnostics);
}

void ManagedApplicationScanTest::invalidManagedSnapshotKeepsNativeRows()
{
    QTemporaryDir root;
    QVERIFY(writeDesktop(root.path(), QStringLiteral("native.fixture"), desktop()));
    auto value = registration(); value.binding.registrationRevision = 0;
    const auto scan = scanApplicationDirectories({root.path()}, {value});
    QCOMPARE(scan.applications.size(), 1);
    QVERIFY(!scan.applications.first().entry.managedBinding);
    QCOMPARE(scan.diagnostics.last().kind, DiagnosticKind::InvalidManagedSnapshot);
}

QTEST_GUILESS_MAIN(ManagedApplicationScanTest)
#include "tst_managed_application_scan.moc"
