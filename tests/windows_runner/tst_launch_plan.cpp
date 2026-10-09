// SPDX-License-Identifier: LGPL-3.0-or-later
#include "test_support.h"

#include <QtTest>

using namespace WindowsRunnerTests;

class WindowsLaunchPlanTests final : public QObject {
    Q_OBJECT
private slots:
    void stableEntrySurvivesRecipeUpdate()
    {
        auto app = application();
        const QString before = windowsCatalogEntryId(app.binding);
        ++app.binding.registrationRevision;
        app.binding.recipeDigest = QString(64, QLatin1Char('b'));
        QCOMPARE(windowsCatalogEntryId(app.binding), before);
        QCOMPARE(before, QStringLiteral("qindaqt.managed.windows.427e0914-558e-4d1e-9b34-001933ef126f"));
    }

    void invalidBinding_data()
    {
        QTest::addColumn<int>("mutation");
        QTest::newRow("empty-id") << 0;
        QTest::newRow("null-uuid") << 1;
        QTest::newRow("uppercase-id") << 2;
        QTest::newRow("braced-id") << 3;
        QTest::newRow("zero-revision") << 4;
        QTest::newRow("short-digest") << 5;
        QTest::newRow("uppercase-digest") << 6;
        QTest::newRow("title-is-not-id") << 7;
    }
    void invalidBinding()
    {
        QFETCH(int, mutation);
        auto app = application();
        switch (mutation) {
        case 0: app.binding.registrationId.clear(); break;
        case 1: app.binding.registrationId = QStringLiteral("00000000-0000-0000-0000-000000000000"); break;
        case 2: app.binding.registrationId = app.binding.registrationId.toUpper(); break;
        case 3: app.binding.registrationId = QStringLiteral("{") + app.binding.registrationId + QStringLiteral("}"); break;
        case 4: app.binding.registrationRevision = 0; break;
        case 5: app.binding.recipeDigest.chop(1); break;
        case 6: app.binding.recipeDigest = app.binding.recipeDigest.toUpper(); break;
        case 7: app.binding.registrationId = QStringLiteral("Notepad"); break;
        }
        QVERIFY(windowsCatalogEntryId(app.binding).isEmpty());
        const auto result = prepareWindowsLaunch(app.binding, app, {});
        QVERIFY(!result.plan);
        QCOMPARE(result.failure, LaunchFailure::InvalidBinding);
    }

    void literalArgumentsAndPrivateProfile()
    {
        auto app = application();
        app.arguments = {QStringLiteral("/private/documents/my note.txt"),
                         QStringLiteral("$(touch /outside)/; quoted value"), QString{}};
        const auto result = prepareWindowsLaunch(app.binding, app, {});
        QVERIFY(result.plan);
        QCOMPARE(result.plan->program, app.runtime.launcher.path);
        QCOMPARE(result.plan->arguments, QStringList{app.application.path} + app.arguments);
        QCOMPARE(result.plan->environment.value(QStringLiteral("WINEPREFIX")), app.prefixPath);
        QCOMPARE(result.plan->environment.value(QStringLiteral("HOME")), app.profileHome);
        QCOMPARE(result.plan->payloads, QVector<PayloadPin>({app.runtime.launcher, app.application}));
    }

    void sessionCannotSteerRuntime()
    {
        const auto app = application();
        QProcessEnvironment session;
        const QStringList forbidden{QStringLiteral("WINEPREFIX"), QStringLiteral("PROTONPATH"),
            QStringLiteral("UMU_NO_PROTON"), QStringLiteral("RUNTIMEPATH"), QStringLiteral("PROTON_VERB"),
            QStringLiteral("LD_PRELOAD"), QStringLiteral("LD_LIBRARY_PATH"), QStringLiteral("LD_AUDIT"),
            QStringLiteral("PYTHONPATH"), QStringLiteral("PYTHONHOME"), QStringLiteral("WINEDLLOVERRIDES"),
            QStringLiteral("STEAM_COMPAT_CLIENT_INSTALL_PATH"), QStringLiteral("HOME"),
            QStringLiteral("SESSION_SECRET")};
        for (const QString &key : forbidden) { session.insert(key, QStringLiteral("hostile")); }
        session.insert(QStringLiteral("DISPLAY"), QStringLiteral(":7"));
        session.insert(QStringLiteral("LC_TIME"), QStringLiteral("C.UTF-8"));
        session.insert(QStringLiteral("PIPEWIRE_REMOTE"), QStringLiteral("pipewire-0"));
        const auto plan = prepareWindowsLaunch(app.binding, app, session).plan;
        QVERIFY(plan);
        for (const QString &key : forbidden) {
            QVERIFY(plan->environment.value(key) != QStringLiteral("hostile"));
        }
        QCOMPARE(plan->environment.value(QStringLiteral("DISPLAY")), QStringLiteral(":7"));
        QCOMPARE(plan->environment.value(QStringLiteral("LC_TIME")), QStringLiteral("C.UTF-8"));
        QCOMPARE(plan->environment.value(QStringLiteral("PIPEWIRE_REMOTE")), QStringLiteral("pipewire-0"));
    }

    void malformedRecipe_data()
    {
        QTest::addColumn<int>("mutation");
        QTest::newRow("relative-executable") << 0;
        QTest::newRow("parent-traversal") << 1;
        QTest::newRow("short-payload-digest") << 2;
        QTest::newRow("empty-runtime-pin") << 3;
        QTest::newRow("invalid-profile-id") << 4;
        QTest::newRow("root-prefix") << 5;
        QTest::newRow("package-control-text") << 6;
        QTest::newRow("unknown-runtime") << 7;
        QTest::newRow("duplicate-payload-path") << 8;
    }
    void malformedRecipe()
    {
        QFETCH(int, mutation);
        auto app = application();
        switch (mutation) {
        case 0: app.application.path = QStringLiteral("notepad.exe"); break;
        case 1: app.application.path = QStringLiteral("/usr/../private/notepad.exe"); break;
        case 2: app.application.sha256.chop(1); break;
        case 3: app.runtime.launcher.bytes = 0; break;
        case 4: app.profileId = QStringLiteral("peer"); break;
        case 5: app.prefixPath = QStringLiteral("/"); break;
        case 6: app.packageCpv = QStringLiteral("app/emulation\nsh"); break;
        case 7: app.runtime.backend = static_cast<RuntimeBackend>(55); break;
        case 8: app.application.path = app.runtime.launcher.path; break;
        }
        const auto result = prepareWindowsLaunch(app.binding, app, {});
        QVERIFY(!result.plan);
        QCOMPARE(result.failure, LaunchFailure::InvalidRecipe);
    }

    void boundedArguments_data()
    {
        QTest::addColumn<int>("mutation");
        QTest::newRow("too-many") << 0;
        QTest::newRow("too-long") << 1;
        QTest::newRow("nul") << 2;
        QTest::newRow("newline") << 3;
    }
    void boundedArguments()
    {
        QFETCH(int, mutation);
        auto app = application();
        switch (mutation) {
        case 0: for (int i = 0; i < 65; ++i) { app.arguments.append(QStringLiteral("arg")); } break;
        case 1: app.arguments = {QString(4097, QLatin1Char('x'))}; break;
        case 2: app.arguments = {QStringLiteral("a") + QChar(0) + QStringLiteral("b")}; break;
        case 3: app.arguments = {QStringLiteral("a\nb")}; break;
        }
        const auto result = prepareWindowsLaunch(app.binding, app, {});
        QVERIFY(!result.plan);
        QCOMPARE(result.failure, LaunchFailure::InvalidRecipe);
    }

    void protonNeedsExactTuple()
    {
        auto app = application();
        app.runtime.backend = RuntimeBackend::ProtonUmu;
        QVERIFY(!prepareWindowsLaunch(app.binding, app, {}).plan);
        app.runtime.launcher.path = QStringLiteral("/usr/bin/umu-run");
        app.runtime.protonDirectory = QStringLiteral("/usr/share/proton/Exact-11");
        app.runtime.protonEntrypoint = {app.runtime.protonDirectory + QStringLiteral("/proton"), QByteArray(32, 'p'), 12000};
        app.runtime.steamRuntimeReceipt = {QStringLiteral("/usr/share/umu/runtime/recipe.json"), QByteArray(32, 's'), 512};
        const auto result = prepareWindowsLaunch(app.binding, app, {});
        QVERIFY(result.plan);
        QCOMPARE(result.plan->payloads.size(), 4);
        QCOMPARE(result.plan->environment.value(QStringLiteral("PROTONPATH")), app.runtime.protonDirectory);
        QCOMPARE(result.plan->environment.value(QStringLiteral("UMU_RUNTIME_UPDATE")), QStringLiteral("0"));
        QCOMPARE(result.plan->environment.value(QStringLiteral("GAMEID")), QStringLiteral("umu-0"));
        app.runtime.protonEntrypoint.path = QStringLiteral("/usr/share/proton/Latest/proton");
        QVERIFY(!prepareWindowsLaunch(app.binding, app, {}).plan);
    }

    void profileRepairAndRecipeUpdateAreVisible()
    {
        auto app = application();
        const auto original = app.binding;
        app.materializationReady = false;
        QCOMPARE(prepareWindowsLaunch(original, app, {}).failure, LaunchFailure::ProfileNotReady);
        app.materializationReady = true;
        app.binding.recipeDigest = QString(64, QLatin1Char('b'));
        QCOMPARE(prepareWindowsLaunch(original, app, {}).failure, LaunchFailure::RegistrationChanged);
    }
};
QTEST_GUILESS_MAIN(WindowsLaunchPlanTests)
#include "tst_launch_plan.moc"
