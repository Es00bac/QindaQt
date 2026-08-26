// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwincommandbuilder.h"

#include <QTemporaryFile>
#include <QtTest>

using namespace QindaQt::Session;

class KWinCommandBuilderTest final : public QObject
{
    Q_OBJECT

private slots:
    void buildsVirtualXWaylandSession();
    void buildsNestedSession();
    void rejectsInvalidBoundaries();
};

void KWinCommandBuilderTest::buildsVirtualXWaylandSession()
{
    QTemporaryFile scenario;
    QVERIFY(scenario.open());
    SessionOptions options;
    options.backend = Backend::Virtual;
    options.outputSize = QSize(2560, 1440);
    options.scale = 1.25;
    options.outputCount = 2;
    options.lockscreen = false;
    options.globalShortcuts = false;
    options.testScenario = scenario.fileName();
    options.sessionExecutable = QStringLiteral("qindaqt-session");

    QString error;
    const auto command = KWinCommandBuilder::build(options, &error);
    QVERIFY2(!command.isEmpty(), qPrintable(error));
    QCOMPARE(command.constFirst(), QStringLiteral("kwin_wayland"));
    QVERIFY(command.contains(QStringLiteral("--virtual")));
    QVERIFY(command.contains(QStringLiteral("--xwayland")));
    QCOMPARE(command[command.indexOf(QStringLiteral("--width")) + 1], QStringLiteral("2560"));
    QCOMPARE(command[command.indexOf(QStringLiteral("--height")) + 1], QStringLiteral("1440"));
    QCOMPARE(command[command.indexOf(QStringLiteral("--scale")) + 1], QStringLiteral("1.25"));
    QCOMPARE(command[command.indexOf(QStringLiteral("--output-count")) + 1],
             QStringLiteral("2"));
    QCOMPARE(command.constLast(), QStringLiteral("qindaqt-session"));
}

void KWinCommandBuilderTest::buildsNestedSession()
{
    SessionOptions options;
    options.backend = Backend::NestedWayland;
    options.parentWaylandDisplay = QStringLiteral("wayland-parent");
    options.xwayland = false;

    QString error;
    const auto command = KWinCommandBuilder::build(options, &error);
    QVERIFY2(!command.isEmpty(), qPrintable(error));
    QCOMPARE(command[command.indexOf(QStringLiteral("--wayland-display")) + 1],
             QStringLiteral("wayland-parent"));
    QVERIFY(!command.contains(QStringLiteral("--xwayland")));
}

void KWinCommandBuilderTest::rejectsInvalidBoundaries()
{
    SessionOptions options;
    QString error;
    options.backend = Backend::NestedWayland;
    QVERIFY(KWinCommandBuilder::build(options, &error).isEmpty());
    QVERIFY(error.contains(QStringLiteral("WAYLAND_DISPLAY")));

    options.backend = Backend::Virtual;
    options.socketName = QStringLiteral("../../live-session");
    QVERIFY(KWinCommandBuilder::build(options, &error).isEmpty());

    options.socketName = QStringLiteral("qindaqt-test");
    options.outputCount = 0;
    QVERIFY(KWinCommandBuilder::build(options, &error).isEmpty());

    options.outputCount = 1;
    options.scale = 8.0;
    QVERIFY(KWinCommandBuilder::build(options, &error).isEmpty());

    options.scale = 1.0;
    options.testScenario = QStringLiteral("/definitely/missing/qindaqt-scenario.json");
    QVERIFY(KWinCommandBuilder::build(options, &error).isEmpty());
}

QTEST_APPLESS_MAIN(KWinCommandBuilderTest)

#include "tst_kwincommandbuilder.moc"
