// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/application_catalog/launch_support.h"

#include <QtTest>

using namespace QindaQt::ApplicationCatalog;

namespace {

QString document(const QString &extraKeys = {},
                 const QString &actions = {})
{
    return QStringLiteral("[Desktop Entry]\n"
                          "Type=Application\n"
                          "Name=Fixture\n"
                          "Exec=fixture --start %u\n")
        + extraKeys + actions;
}

} // namespace

class LaunchSupportTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void plansAPlainProcessSpawn();
    void routesTerminalAndDbusVariants();
    void failsClosedOnBrokenDocuments();
    void buildsTerminalCommandLines();
};

void LaunchSupportTests::plansAPlainProcessSpawn()
{
    const auto preparation = planApplicationLaunch(
        document(), {}, QStringLiteral("Fixture"),
        QStringLiteral("/data/applications/fixture.desktop"));
    QCOMPARE(preparation.support, LaunchSupport::ProcessSpawn);
    QVERIFY(preparation.spawnable());
    // %u is a droppable field code for this caller (no file argument is ever
    // supplied), so the argv is exactly the program plus its static flag.
    QCOMPARE(preparation.program, QStringLiteral("fixture"));
    QCOMPARE(preparation.arguments, QStringList{QStringLiteral("--start")});
    QVERIFY(preparation.message.isEmpty());
}

void LaunchSupportTests::routesTerminalAndDbusVariants()
{
    const auto terminal = planApplicationLaunch(
        document(QStringLiteral("Terminal=true\n")), {},
        QStringLiteral("Fixture"), {});
    QCOMPARE(terminal.support, LaunchSupport::TerminalRequired);
    QVERIFY(terminal.spawnable());
    QCOMPARE(terminal.program, QStringLiteral("fixture"));

    const auto dbus = planApplicationLaunch(
        document(QStringLiteral("DBusActivatable=true\n")), {},
        QStringLiteral("Fixture"), {});
    QCOMPARE(dbus.support, LaunchSupport::DbusActivatable);
    QVERIFY(!dbus.spawnable());
}

void LaunchSupportTests::failsClosedOnBrokenDocuments()
{
    const auto missingExec = planApplicationLaunch(
        QStringLiteral("[Desktop Entry]\nType=Application\nName=Fixture\n"),
        {}, QStringLiteral("Fixture"), {});
    QCOMPARE(missingExec.support, LaunchSupport::Unsupported);
    QVERIFY(!missingExec.message.isEmpty());

    const auto unknownAction = planApplicationLaunch(
        document({}, QStringLiteral("\n[Desktop Action win]\n"
                                    "Name=Win\nExec=fixture --win\n")),
        QStringLiteral("missing"), QStringLiteral("Fixture"), {});
    QCOMPARE(unknownAction.support, LaunchSupport::Unsupported);

    const auto action = planApplicationLaunch(
        document({}, QStringLiteral("\n[Desktop Action win]\n"
                                    "Name=Win\nExec=fixture --win\n")),
        QStringLiteral("win"), QStringLiteral("Fixture"), {});
    QCOMPARE(action.support, LaunchSupport::ProcessSpawn);
    QCOMPARE(action.arguments, QStringList{QStringLiteral("--win")});
}

void LaunchSupportTests::buildsTerminalCommandLines()
{
    const auto command = terminalCommandLine(
        {QStringLiteral("qqterm"), QStringLiteral("-e")},
        QStringLiteral("fixture"), {QStringLiteral("--start")});
    const QStringList expected{QStringLiteral("qqterm"),
                               QStringLiteral("-e"),
                               QStringLiteral("fixture"),
                               QStringLiteral("--start")};
    QCOMPARE(command, expected);
    QVERIFY(terminalCommandLine({}, QStringLiteral("fixture"), {}).isEmpty());
    QVERIFY(terminalCommandLine({QStringLiteral("qqterm")}, {}, {}).isEmpty());
}

QTEST_GUILESS_MAIN(LaunchSupportTests)
#include "tst_launch_support.moc"
