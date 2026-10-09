// SPDX-License-Identifier: GPL-3.0-or-later
#include "managed_catalog_fixtures.h"
#include "qindaqt/application_catalog/application_directory_scan.h"
#include "qindaqt/application_catalog/launch_support.h"

#include <QTest>

using namespace QindaQt::ApplicationCatalog;
using namespace ManagedCatalogFixtures;

namespace {
ScannedApplication managedRow()
{
    const auto value = registration();
    ScannedApplication row;
    row.entry.id = managedApplicationEntryId(value.binding);
    row.entry.name = value.name;
    row.entry.managedBinding = value.binding;
    return row;
}
}

class ManagedApplicationLaunchTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void managedPlanContainsOnlyRegistrationBinding();
    void staleDocumentCannotBecomeManagedExec();
    void malformedAndMismatchedBindingsRefuse();
    void unavailableFileAndActionHandoffsRefuse();
    void nativeRowUsesExistingPurePlanner();
};

void ManagedApplicationLaunchTest::managedPlanContainsOnlyRegistrationBinding()
{
    const auto row = managedRow();
    const auto plan = planApplicationLaunch(row);
    QCOMPARE(plan.support, LaunchSupport::ManagedRuntime);
    QVERIFY(!plan.spawnable());
    QVERIFY(plan.program.isEmpty());
    QVERIFY(plan.arguments.isEmpty());
    QCOMPARE(plan.managedBinding, row.entry.managedBinding);
    QCOMPARE(plan.fileArguments, qsizetype(0));
}

void ManagedApplicationLaunchTest::staleDocumentCannotBecomeManagedExec()
{
    auto row = managedRow();
    row.documentText = desktop();
    QCOMPARE(planApplicationLaunch(row).support, LaunchSupport::Unsupported);
    row.documentText.clear();
    row.desktopFilePath = QStringLiteral("/arbitrary/path.desktop");
    QCOMPARE(planApplicationLaunch(row).support, LaunchSupport::Unsupported);
}

void ManagedApplicationLaunchTest::malformedAndMismatchedBindingsRefuse()
{
    auto row = managedRow();
    row.entry.managedBinding->registrationRevision = 0;
    QCOMPARE(planApplicationLaunch(row).support, LaunchSupport::Unsupported);
    row = managedRow(); row.entry.id = QStringLiteral("native.other");
    QCOMPARE(planApplicationLaunch(row).support, LaunchSupport::Unsupported);
}

void ManagedApplicationLaunchTest::unavailableFileAndActionHandoffsRefuse()
{
    const auto row = managedRow();
    QCOMPARE(planApplicationLaunch(row, QStringLiteral("arbitrary")).support,
             LaunchSupport::Unsupported);
    QCOMPARE(planApplicationLaunch(row, {}, {QStringLiteral("/document.txt")}).support,
             LaunchSupport::Unsupported);
}

void ManagedApplicationLaunchTest::nativeRowUsesExistingPurePlanner()
{
    ScannedApplication row;
    row.entry.name = QStringLiteral("Native Fixture");
    row.documentText = desktop();
    row.desktopFilePath = QStringLiteral("/fixture.desktop");
    const auto plan = planApplicationLaunch(row);
    QCOMPARE(plan.support, LaunchSupport::ProcessSpawn);
    QVERIFY(plan.spawnable());
    QCOMPARE(plan.program, QStringLiteral("/bin/true"));
    QVERIFY(!plan.managedBinding);
}

QTEST_GUILESS_MAIN(ManagedApplicationLaunchTest)
#include "tst_managed_application_launch.moc"
