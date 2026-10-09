// SPDX-License-Identifier: LGPL-3.0-or-later
#include "test_support.h"

#include <QtTest>

using namespace WindowsRunnerTests;

class WindowsApplicationRunnerTests final : public QObject {
    Q_OBJECT
private slots:
    void acceptedDispatchOnlyReportsStarting()
    {
        Registrations registrations; Payloads payloads; Launcher launcher;
        WindowsApplicationRunner runner(registrations, payloads, launcher);
        const auto result = runner.launch(registrations.current->binding);
        QCOMPARE(result.state, LaunchState::Starting);
        QCOMPARE(result.failure, LaunchFailure::None);
        QCOMPARE(result.catalogEntryId, windowsCatalogEntryId(registrations.current->binding));
        QVERIFY(!result.launchId.isEmpty());
        QCOMPARE(launcher.submissions.size(), 1);
    }

    void missingRegistrationAndInvalidBindingDoNotDispatch()
    {
        Registrations registrations; Payloads payloads; Launcher launcher;
        WindowsApplicationRunner runner(registrations, payloads, launcher);
        const auto binding = registrations.current->binding;
        registrations.current.reset();
        QCOMPARE(runner.launch(binding).failure, LaunchFailure::MissingRegistration);
        QCOMPARE(runner.launch({}).failure, LaunchFailure::InvalidBinding);
        QCOMPARE(payloads.acquisitions, 0);
        QVERIFY(launcher.submissions.isEmpty());
    }

    void staleCatalogRevisionCannotDispatch()
    {
        Registrations registrations; Payloads payloads; Launcher launcher;
        WindowsApplicationRunner runner(registrations, payloads, launcher);
        const auto binding = registrations.current->binding;
        ++registrations.current->binding.registrationRevision;
        QCOMPARE(runner.launch(binding).failure, LaunchFailure::RegistrationChanged);
        QCOMPARE(payloads.acquisitions, 0);
        QVERIFY(launcher.submissions.isEmpty());
    }

    void changedPayload_data()
    {
        QTest::addColumn<int>("mutation");
        QTest::newRow("runtime-replaced") << 0;
        QTest::newRow("application-replaced") << 1;
        QTest::newRow("size-changed") << 2;
        QTest::newRow("unexpected-file") << 3;
    }
    void changedPayload()
    {
        QFETCH(int, mutation);
        Registrations registrations; Payloads payloads; Launcher launcher;
        WindowsApplicationRunner runner(registrations, payloads, launcher);
        payloads.amend = [mutation](Lease &lease) {
            switch (mutation) {
            case 0: lease.pins[0].sha256 = QByteArray(32, 'z'); break;
            case 1: lease.pins[1].sha256 = QByteArray(32, 'z'); break;
            case 2: ++lease.pins[1].bytes; break;
            case 3: lease.pins.append({QStringLiteral("/unexpected"), QByteArray(32, 'z'), 100}); break;
            }
        };
        QCOMPARE(runner.launch(registrations.current->binding).failure, LaunchFailure::PayloadChanged);
        QVERIFY(launcher.submissions.isEmpty());
    }

    void removalDuringPayloadInspectionDoesNotDispatch()
    {
        Registrations registrations; Payloads payloads; Launcher launcher;
        WindowsApplicationRunner runner(registrations, payloads, launcher);
        const auto binding = registrations.current->binding;
        payloads.onAcquire = [&registrations] { registrations.current.reset(); };
        QCOMPARE(runner.launch(binding).failure, LaunchFailure::RegistrationChanged);
        QVERIFY(launcher.submissions.isEmpty());
    }

    void sameRevisionRecipeMutationDoesNotDispatch()
    {
        Registrations registrations; Payloads payloads; Launcher launcher;
        WindowsApplicationRunner runner(registrations, payloads, launcher);
        payloads.onAcquire = [&registrations] {
            registrations.current->prefixPath = QStringLiteral("/private/peer/prefix");
        };
        QCOMPARE(runner.launch(registrations.current->binding).failure, LaunchFailure::RegistrationChanged);
        QVERIFY(launcher.submissions.isEmpty());
    }

    void latePayloadReplacementDoesNotDispatch()
    {
        Registrations registrations; Payloads payloads; Launcher launcher;
        WindowsApplicationRunner runner(registrations, payloads, launcher);
        payloads.amend = [](Lease &lease) { lease.freshness = [](int check) { return check < 2; }; };
        QCOMPARE(runner.launch(registrations.current->binding).failure, LaunchFailure::PayloadChanged);
        QVERIFY(launcher.submissions.isEmpty());
    }

    void dispatchFailureDoesNotReplay_data()
    {
        QTest::addColumn<int>("outcome");
        QTest::newRow("refused") << 0;
        QTest::newRow("uncertain") << 1;
    }
    void dispatchFailureDoesNotReplay()
    {
        QFETCH(int, outcome);
        Registrations registrations; Payloads payloads; Launcher launcher;
        WindowsApplicationRunner runner(registrations, payloads, launcher);
        launcher.outcome = outcome == 0 ? LaunchDispatch::Refused : LaunchDispatch::OutcomeUnknown;
        const auto result = runner.launch(registrations.current->binding);
        QCOMPARE(result.failure, outcome == 0 ? LaunchFailure::DispatchRefused : LaunchFailure::DispatchUncertain);
        QCOMPARE(result.state, outcome == 0 ? LaunchState::Unavailable : LaunchState::OutcomeUnknown);
        QCOMPARE(launcher.submissions.size(), 1);
        QVERIFY(!result.message.contains(registrations.current->prefixPath));
    }

    void explicitRelaunchKeepsEntryAndCreatesNewLaunch()
    {
        Registrations registrations; Payloads payloads; Launcher launcher;
        WindowsApplicationRunner runner(registrations, payloads, launcher);
        const auto first = runner.launch(registrations.current->binding);
        const auto second = runner.launch(registrations.current->binding);
        QCOMPARE(second.catalogEntryId, first.catalogEntryId);
        QVERIFY(first.launchId != second.launchId);
        QCOMPARE(launcher.submissions.size(), 2);
    }

    void missingPayloadVerificationFailsClosed()
    {
        Registrations registrations; Payloads payloads; Launcher launcher;
        WindowsApplicationRunner runner(registrations, payloads, launcher);
        payloads.available = false;
        QCOMPARE(runner.launch(registrations.current->binding).failure, LaunchFailure::PayloadChanged);
        QVERIFY(launcher.submissions.isEmpty());
    }
};
QTEST_GUILESS_MAIN(WindowsApplicationRunnerTests)
#include "tst_windows_runner.moc"
