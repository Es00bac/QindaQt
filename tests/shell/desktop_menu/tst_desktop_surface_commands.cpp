// SPDX-License-Identifier: GPL-3.0-or-later

// ADR-0260: the desktop menu's channel to the desktop-icons surfaces. The QML
// half (DesktopSurface.qml) is exercised by the desktop-surface rows; this
// row pins the shell half's attachment, primary, and paste truth.

#include "qindaqt/shell/desktop_menu/desktop_surface_commands.h"

#include <QSignalSpy>
#include <QtTest>

using QindaQt::Shell::DesktopMenu::DesktopSurfaceCommands;

class DesktopSurfaceCommandsTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void refusesWithoutAPrimarySurface();
    void primaryOnlyCommandsAreMarked();
    void pasteTruthComesOnlyFromTheAttachedPrimary();
};

void DesktopSurfaceCommandsTest::refusesWithoutAPrimarySurface()
{
    DesktopSurfaceCommands commands;
    QSignalSpy requested(&commands, &DesktopSurfaceCommands::commandRequested);
    QVERIFY(!commands.surfaceAttached());
    QVERIFY(!commands.request(DesktopSurfaceCommands::Command::NewFolder));
    commands.attachSurface(QStringLiteral("DP-2"), false);
    QVERIFY(!commands.surfaceAttached());
    QVERIFY(!commands.request(DesktopSurfaceCommands::Command::SelectAll));
    QCOMPARE(requested.size(), 0);
    commands.attachSurface(QString{}, true);
    QVERIFY(!commands.surfaceAttached());
}

void DesktopSurfaceCommandsTest::primaryOnlyCommandsAreMarked()
{
    DesktopSurfaceCommands commands;
    QSignalSpy state(&commands, &DesktopSurfaceCommands::stateChanged);
    commands.attachSurface(QStringLiteral("DP-1"), true);
    commands.attachSurface(QStringLiteral("DP-2"), false);
    QVERIFY(commands.surfaceAttached());
    QCOMPARE(state.size(), 1);
    QSignalSpy requested(&commands, &DesktopSurfaceCommands::commandRequested);
    QVERIFY(commands.request(DesktopSurfaceCommands::Command::NewFolder));
    QVERIFY(commands.request(DesktopSurfaceCommands::Command::Paste));
    QVERIFY(commands.request(DesktopSurfaceCommands::Command::SelectAll));
    QVERIFY(commands.request(DesktopSurfaceCommands::Command::CleanUp));
    QCOMPARE(requested.size(), 4);
    QCOMPARE(requested.at(0), (QList<QVariant>{QStringLiteral("new-folder"), true}));
    QCOMPARE(requested.at(1), (QList<QVariant>{QStringLiteral("paste"), true}));
    QCOMPARE(requested.at(2), (QList<QVariant>{QStringLiteral("select-all"), false}));
    QCOMPARE(requested.at(3), (QList<QVariant>{QStringLiteral("clean-up"), true}));

    // The primary output moving to another surface keeps the channel usable.
    commands.attachSurface(QStringLiteral("DP-1"), false);
    QVERIFY(!commands.surfaceAttached());
    commands.attachSurface(QStringLiteral("DP-2"), true);
    QVERIFY(commands.surfaceAttached());
    commands.detachSurface(QStringLiteral("DP-2"));
    QVERIFY(!commands.surfaceAttached());
}

void DesktopSurfaceCommandsTest::pasteTruthComesOnlyFromTheAttachedPrimary()
{
    DesktopSurfaceCommands commands;
    commands.attachSurface(QStringLiteral("DP-1"), true);
    commands.attachSurface(QStringLiteral("DP-2"), false);
    commands.reportPasteAvailable(QStringLiteral("DP-2"), true);
    QVERIFY(!commands.pasteAvailable());
    commands.reportPasteAvailable(QStringLiteral("unknown"), true);
    QVERIFY(!commands.pasteAvailable());
    QSignalSpy state(&commands, &DesktopSurfaceCommands::stateChanged);
    commands.reportPasteAvailable(QStringLiteral("DP-1"), true);
    QVERIFY(commands.pasteAvailable());
    QCOMPARE(state.size(), 1);
    // Losing primary status or detaching drops the report with it.
    commands.attachSurface(QStringLiteral("DP-1"), false);
    QVERIFY(!commands.pasteAvailable());
    commands.attachSurface(QStringLiteral("DP-1"), true);
    QVERIFY(!commands.pasteAvailable());
    commands.reportPasteAvailable(QStringLiteral("DP-1"), true);
    commands.detachSurface(QStringLiteral("DP-1"));
    QVERIFY(!commands.pasteAvailable());
}

QTEST_GUILESS_MAIN(DesktopSurfaceCommandsTest)

#include "tst_desktop_surface_commands.moc"
