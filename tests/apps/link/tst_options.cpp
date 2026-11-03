// SPDX-License-Identifier: GPL-3.0-or-later
// Command line of qindaqt-link (reply mode and `--`), and the AppShell action catalog.
#include "app_options.h"
#include "link_actions.h"

#include <qindaqt/app_shell/application_coordinator.h>

#include <QtTest/QtTest>

using namespace QindaQt::LinkApp;

namespace {
std::optional<AppOptions> parse(const QStringList &args, QString *error = nullptr)
{
    QString local;
    return parseAppOptions(QStringList{QStringLiteral("qindaqt-link")} + args, error != nullptr ? error : &local);
}
}

class OptionsTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void defaultsOpenTheDevicesPage()
    {
        const auto options = parse({});
        QVERIFY(options);
        QCOMPARE(options->mode, Mode::Main);
        QCOMPARE(options->page, QStringLiteral("devices"));
        QVERIFY(!options->fake());
        QVERIFY(!options->headless());
    }

    void replyModeTakesTheTargetAfterTheDoubleDash()
    {
        // Exactly the argv the notification path launches (ProcessReplyLauncher).
        const auto options = parse({QStringLiteral("--reply"), QStringLiteral("--"), QStringLiteral("dev1"), QStringLiteral("0|com.x|7|null|1"), QStringLiteral("2")});
        QVERIFY(options);
        QCOMPARE(options->mode, Mode::Reply);
        QCOMPARE(options->device, QStringLiteral("dev1"));
        QCOMPARE(options->key, QStringLiteral("0|com.x|7|null|1"));
        QCOMPARE(options->action, QStringLiteral("2"));
    }

    void theDoubleDashKeepsDashedValuesFromBeingOptions()
    {
        const auto options = parse({QStringLiteral("--reply"), QStringLiteral("--"), QStringLiteral("-dev"), QStringLiteral("--key"), QStringLiteral("-1")});
        QVERIFY(options);
        QCOMPARE(options->device, QStringLiteral("-dev"));
        QCOMPARE(options->key, QStringLiteral("--key"));
        QCOMPARE(options->action, QStringLiteral("-1"));
    }

    void replyModeNeedsExactlyThreeNonEmptyValues()
    {
        QString error;
        QVERIFY(!parse({QStringLiteral("--reply"), QStringLiteral("--"), QStringLiteral("a"), QStringLiteral("b")}, &error));
        QVERIFY(error.contains(QStringLiteral("--reply")));
        QVERIFY(!parse({QStringLiteral("--reply"), QStringLiteral("--"), QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c"), QStringLiteral("d")}));
        QVERIFY(!parse({QStringLiteral("--reply"), QStringLiteral("--"), QStringLiteral("a"), QString(), QStringLiteral("c")}));
        QVERIFY(!parse({QStringLiteral("stray")}));
    }

    void pageSizeAndRenderOptionsAreValidated()
    {
        const auto ok = parse({QStringLiteral("--page"), QStringLiteral("Messages"), QStringLiteral("--size"), QStringLiteral("900x600"),
                               QStringLiteral("--fake"), QStringLiteral("empty"), QStringLiteral("--grab"), QStringLiteral("/tmp/x.png")});
        QVERIFY(ok);
        QCOMPARE(ok->page, QStringLiteral("messages"));
        QCOMPARE(ok->size, QSize(900, 600));
        QVERIFY(ok->headless());
        QVERIFY(!parse({QStringLiteral("--page"), QStringLiteral("nowhere")}));
        QVERIFY(!parse({QStringLiteral("--size"), QStringLiteral("10x10")}));
        QVERIFY(!parse({QStringLiteral("--fake"), QStringLiteral("everything")}));
    }

    void aRenderMustUseScriptedData()
    {
        QString error;
        QVERIFY(!parse({QStringLiteral("--grab"), QStringLiteral("/tmp/x.png")}, &error));
        QVERIFY(error.contains(QStringLiteral("--fake")));
    }

    void everyPageHasAMenuActionWithAShortcutAndTheCatalogIsAccepted()
    {
        QindaQt::AppShell::ApplicationCoordinator coordinator;
        coordinator.setApplicationName(QStringLiteral("QindaLink"));
        const auto status = coordinator.replaceActions(linkActions());
        QVERIFY2(status.ok(), qPrintable(status.message));
        QStringList pages;
        for (const auto &action : linkActions()) {
            QVERIFY(!action.shortcut.isEmpty());
            if (!pageForActionId(action.id).isEmpty()) pages << pageForActionId(action.id);
        }
        pages.sort();
        QStringList expected = pageIds();
        expected.sort();
        QCOMPARE(pages, expected);
    }
};

QTEST_GUILESS_MAIN(OptionsTest)
#include "tst_options.moc"
