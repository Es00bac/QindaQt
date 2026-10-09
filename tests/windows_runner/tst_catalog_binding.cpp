// SPDX-License-Identifier: LGPL-3.0-or-later
#include "test_support.h"
#include <qindaqt/windows_runner/catalog_binding.h>
#include <QtTest>
using namespace WindowsRunnerTests;

class CatalogBindingTests final : public QObject {
    Q_OBJECT
private slots:
    void acceptedWindowsBindingReachesCurrentRecipe()
    {
        Registrations registrations; Payloads payloads; Launcher launcher;
        WindowsApplicationRunner runner(registrations, payloads, launcher);
        const auto local = registrations.current->binding;
        const QindaQt::ShellLauncher::ManagedApplicationBinding binding{
            QindaQt::ShellLauncher::ApplicationOrigin::Windows,
            local.registrationId, local.registrationRevision, local.recipeDigest};
        QCOMPARE(launchManagedWindowsApplication(runner, binding).state, LaunchState::Starting);
        QCOMPARE(launcher.submissions.constFirst().catalogEntryId,
                 QindaQt::ShellLauncher::managedApplicationEntryId(binding));
    }
    void otherOrigins_data()
    {
        QTest::addColumn<int>("origin");
        QTest::newRow("native") << 0; QTest::newRow("android") << 1;
        QTest::newRow("unknown") << 99;
    }
    void otherOrigins()
    {
        QFETCH(int, origin);
        Registrations registrations; Payloads payloads; Launcher launcher;
        WindowsApplicationRunner runner(registrations, payloads, launcher);
        const auto local = registrations.current->binding;
        QindaQt::ShellLauncher::ManagedApplicationBinding binding{
            static_cast<QindaQt::ShellLauncher::ApplicationOrigin>(origin),
            local.registrationId, local.registrationRevision, local.recipeDigest};
        QVERIFY(!windowsBinding(binding));
        QCOMPARE(launchManagedWindowsApplication(runner, binding).failure, LaunchFailure::InvalidBinding);
        QCOMPARE(registrations.reads, 0); QVERIFY(launcher.submissions.isEmpty());
    }
    void staleRecipeCannotBecomeCurrentByConversion()
    {
        Registrations registrations; Payloads payloads; Launcher launcher;
        WindowsApplicationRunner runner(registrations, payloads, launcher);
        const auto local = registrations.current->binding;
        const QindaQt::ShellLauncher::ManagedApplicationBinding binding{
            QindaQt::ShellLauncher::ApplicationOrigin::Windows,
            local.registrationId, local.registrationRevision - 1, local.recipeDigest};
        QCOMPARE(launchManagedWindowsApplication(runner, binding).failure, LaunchFailure::RegistrationChanged);
        QVERIFY(launcher.submissions.isEmpty());
    }
};
QTEST_GUILESS_MAIN(CatalogBindingTests)
#include "tst_catalog_binding.moc"
