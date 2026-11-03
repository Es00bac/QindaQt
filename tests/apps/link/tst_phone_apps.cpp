// SPDX-License-Identifier: GPL-3.0-or-later
// Phone apps and notification mute list over fakes: inventory parsing, favourites persistence,
// the mirror argv, the "needs USB" state, mute toggling and the Settings macros launch.
#include "app_preferences.h"
#include "fake_link_api.h"
#include "launcher.h"
#include "notifications_view_model.h"
#include "phone_apps_view_model.h"
#include "process_runner.h"

#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

using namespace QindaQt::LinkApp;

class PhoneAppsTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void parsesInventoryAndSkipsBadPackages()
    {
        const QByteArray out = "diagnostic line\n"
                               "{\"apps\":[{\"package\":\"com.a.b\",\"label\":\"Alpha\",\"icon\":\"AAAA\"},"
                               "{\"package\":\"not a package\",\"label\":\"Bad\"},"
                               "{\"package\":\"org.x.y\",\"icon\":\"\"}]}\n";
        const QList<QVariantMap> rows = PhoneAppsViewModel::parseInventory(out);
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows.at(0).value(QStringLiteral("iconUrl")).toString(), QStringLiteral("data:image/png;base64,AAAA"));
        QCOMPARE(rows.at(1).value(QStringLiteral("label")).toString(), QStringLiteral("org.x.y"));  // label falls back to the package
        QVERIFY(rows.at(1).value(QStringLiteral("iconUrl")).toString().isEmpty());
        QVERIFY(PhoneAppsViewModel::parseInventory("garbage").isEmpty());
    }

    void listsThroughTheMirrorWithTheServicesSerial()
    {
        QTemporaryDir dir;
        FakeLinkApi api;
        FakeProcessRunner runner;
        runner.captureOutput = FakeLinkApi::inventoryJson();
        AppPreferences prefs(dir.filePath(QStringLiteral("app.json")));
        PhoneAppsViewModel apps(api, runner, prefs);
        apps.setMirrorProgram(QStringLiteral("mirror"));
        apps.refresh();
        QTRY_COMPARE(apps.state(), QStringLiteral("ready"));
        QCOMPARE(apps.totalCount(), 8);
        QCOMPARE(runner.captured, (QList<QStringList>{{QStringLiteral("mirror"), QStringLiteral("--serial"), QStringLiteral("DEMO0001"),
                                                       QStringLiteral("--list-json")}}));
        QCOMPARE(apps.apps()->get(0).value(QStringLiteral("label")).toString(), QStringLiteral("Bank"));  // alphabetical

        apps.setSearchText(QStringLiteral("MAP"));
        QCOMPARE(apps.apps()->count(), 1);
        apps.setSearchText(QString());
    }

    void favouritesSortFirstAndPersist()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("app.json"));
        FakeLinkApi api;
        FakeProcessRunner runner;
        runner.captureOutput = FakeLinkApi::inventoryJson();
        {
            AppPreferences prefs(path);
            PhoneAppsViewModel apps(api, runner, prefs);
            apps.refresh();
            QTRY_COMPARE(apps.state(), QStringLiteral("ready"));
            apps.toggleFavourite(QStringLiteral("com.example.music"));
            QCOMPARE(apps.apps()->get(0).value(QStringLiteral("pkg")).toString(), QStringLiteral("com.example.music"));
            QVERIFY(apps.apps()->get(0).value(QStringLiteral("favourite")).toBool());
        }
        AppPreferences reread(path);
        QVERIFY(reread.isFavourite(QStringLiteral("com.example.music")));
        reread.setFavourite(QStringLiteral("com.example.music"), false);
        QVERIFY(!AppPreferences(path).isFavourite(QStringLiteral("com.example.music")));
    }

    void opensEachAppAsItsOwnMirrorProcess()
    {
        QTemporaryDir dir;
        FakeLinkApi api;
        FakeProcessRunner runner;
        runner.captureOutput = FakeLinkApi::inventoryJson();
        AppPreferences prefs(dir.filePath(QStringLiteral("app.json")));
        PhoneAppsViewModel apps(api, runner, prefs);
        apps.setMirrorProgram(QStringLiteral("mirror"));
        apps.refresh();
        QTRY_COMPARE(apps.state(), QStringLiteral("ready"));
        QVERIFY(apps.open(QStringLiteral("com.example.chat")));
        QCOMPARE(runner.started, (QList<QStringList>{{QStringLiteral("mirror"), QStringLiteral("--serial"), QStringLiteral("DEMO0001"),
                                                      QStringLiteral("--package"), QStringLiteral("com.example.chat")}}));
        QVERIFY(!apps.open(QStringLiteral("bad name; rm")));  // never reaches a process
        QCOMPARE(runner.started.size(), 1);
    }

    void withoutAnAdbPhoneItAsksForUsbAndStartsNothing()
    {
        QTemporaryDir dir;
        FakeLinkApi api;
        api.adb = QJsonArray();
        FakeProcessRunner runner;
        AppPreferences prefs(dir.filePath(QStringLiteral("app.json")));
        PhoneAppsViewModel apps(api, runner, prefs);
        apps.refresh();
        QTRY_COMPARE(apps.state(), QStringLiteral("needs-adb"));
        QVERIFY(runner.captured.isEmpty());
        QVERIFY(!apps.open(QStringLiteral("com.example.chat")));
        QVERIFY(runner.started.isEmpty());
    }

    void aFailedMirrorListingIsAnErrorState()
    {
        QTemporaryDir dir;
        FakeLinkApi api;
        FakeProcessRunner runner;
        runner.captureExit = 1;
        AppPreferences prefs(dir.filePath(QStringLiteral("app.json")));
        PhoneAppsViewModel apps(api, runner, prefs);
        apps.refresh();
        QTRY_COMPARE(apps.state(), QStringLiteral("error"));
    }

    void muteListTogglesAndTellsTheService()
    {
        FakeLinkApi api;
        NotificationsViewModel mutes(api);
        mutes.refresh();
        QTRY_COMPARE(mutes.totalCount(), 5);
        QCOMPARE(mutes.mutedCount(), 2);
        mutes.setMuted(QStringLiteral("com.example.chat"), true);
        QCOMPARE(mutes.mutedCount(), 3);
        QCOMPARE(api.muted.value(QStringLiteral("com.example.chat")), true);
        mutes.setMuted(QStringLiteral("com.example.news"), false);
        QCOMPARE(api.muted.value(QStringLiteral("com.example.news"), true), false);
        QCOMPARE(mutes.mutedCount(), 2);
        mutes.setSearchText(QStringLiteral("game"));
        QCOMPARE(mutes.apps()->count(), 1);
        mutes.refresh();  // the service remembers the choices
        QTRY_COMPARE(mutes.mutedCount(), 2);
    }

    void macrosButtonOpensSettingsAtTheMacrosDestination()
    {
        FakeProcessRunner runner;
        Launcher launcher(runner);
        QVERIFY(launcher.openMacroSettings());
        QCOMPARE(runner.started, (QList<QStringList>{{QStringLiteral("qindaqt-settings"), QStringLiteral("--page"), QStringLiteral("input"),
                                                      QStringLiteral("--destination"), QStringLiteral("macros")}}));
    }

    void rcsNoticeDismissalPersists()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("sub/app.json"));
        {
            AppPreferences prefs(path);
            QVERIFY(!prefs.rcsNoticeDismissed());
            prefs.dismissRcsNotice(true);
        }
        QVERIFY(AppPreferences(path).rcsNoticeDismissed());
    }
};

QTEST_GUILESS_MAIN(PhoneAppsTest)
#include "tst_phone_apps.moc"
