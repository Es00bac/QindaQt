// SPDX-License-Identifier: GPL-3.0-or-later

// ADR-0169: windows whose reported class is an opaque launcher artifact are
// reported as the program actually behind them. The motivating case is measured
// live, not invented: Battle.net under umu/Proton reports
// WM_CLASS = ("steam_app_0", "steam_app_0") while its command line names
// C:\Program Files (x86)\Battle.net\Battle.net.exe, and the host's own
// battlenet.desktop declares StartupWMClass=battle.net.exe.

#include "qindaqt/compositor/foreignwindowidentity.h"

#include <QtTest>

using namespace QindaQt::Compositor;

namespace {

// /proc/<pid>/cmdline is NUL-separated with a trailing NUL.
QByteArray commandLine(const QList<QByteArray> &arguments)
{
    QByteArray joined;
    for (const QByteArray &argument : arguments) {
        joined.append(argument);
        joined.append('\0');
    }
    return joined;
}

} // namespace

class ForeignWindowIdentityTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void onlyLauncherArtifactClassesAreTreatedAsOpaque();
    void realClassesAreNeverOverridden();
    void theLiveBattleNetCaseResolvesToItsExecutable();
    void theLastWindowsExecutableWinsOverLauncherShims();
    void nativeClientsFallBackToArgvZero();
    void hostileOrAbsentCommandLinesYieldNothing();
};

void ForeignWindowIdentityTests::onlyLauncherArtifactClassesAreTreatedAsOpaque()
{
    QVERIFY(isOpaqueLauncherClass(QString()));
    QVERIFY(isOpaqueLauncherClass(QStringLiteral("   ")));
    QVERIFY(isOpaqueLauncherClass(QStringLiteral("steam_app_0")));
    QVERIFY(isOpaqueLauncherClass(QStringLiteral("steam_app_220")));
    QVERIFY(isOpaqueLauncherClass(QStringLiteral("STEAM_APP_42")));
    QVERIFY(isOpaqueLauncherClass(QStringLiteral("explorer.exe")));
    QVERIFY(isOpaqueLauncherClass(QStringLiteral("wine")));
    QVERIFY(isOpaqueLauncherClass(QStringLiteral("winemenubuilder.exe")));
}

// A real class is always better identity than anything derived, so the repair
// must never fire for one.
void ForeignWindowIdentityTests::realClassesAreNeverOverridden()
{
    QVERIFY(!isOpaqueLauncherClass(QStringLiteral("firefox")));
    QVERIFY(!isOpaqueLauncherClass(QStringLiteral("battle.net.exe")));
    QVERIFY(!isOpaqueLauncherClass(QStringLiteral("steam")));
    QVERIFY(!isOpaqueLauncherClass(QStringLiteral("steam_app_")));
    QVERIFY(!isOpaqueLauncherClass(QStringLiteral("steam_app_x1")));
    QVERIFY(!isOpaqueLauncherClass(QStringLiteral("org.qindaqt.Terminal")));

    const auto cmdline = commandLine({"/usr/bin/firefox"});
    QCOMPARE(resolveApplicationId(QString(), QStringLiteral("firefox"),
                                  executableFromCommandLine(cmdline)),
             QStringLiteral("firefox"));
    // ADR-0230 added the Steam manifest name as a fourth input; an empty
    // one means "no manifest", which is every non-Steam window.
    QCOMPARE(resolveApplicationName(QStringLiteral("firefox"),
                                    QStringLiteral("firefox"),
                                    QStringLiteral("firefox"), QString()),
             QStringLiteral("firefox"));
    // A declared desktop file name outranks everything, unchanged.
    QCOMPARE(resolveApplicationId(QStringLiteral("org.mozilla.firefox"),
                                  QStringLiteral("steam_app_0"),
                                  QStringLiteral("Battle.net.exe")),
             QStringLiteral("org.mozilla.firefox"));
}

void ForeignWindowIdentityTests::theLiveBattleNetCaseResolvesToItsExecutable()
{
    const auto cmdline = commandLine(
        {R"(C:\Program Files (x86)\Battle.net\Battle.net.exe)", "--from-launcher"});
    const QString executable = executableFromCommandLine(cmdline);
    QCOMPARE(executable, QStringLiteral("Battle.net.exe"));

    // The reported id becomes the key the host's battlenet.desktop already
    // matches through StartupWMClass, instead of "steam_app_0".
    QCOMPARE(resolveApplicationId(QString(), QStringLiteral("steam_app_0"),
                                  executable),
             QStringLiteral("Battle.net.exe"));
    QCOMPARE(resolveApplicationName(QStringLiteral("steam_app_0"), executable,
                                    QStringLiteral("Battle.net.exe"), QString()),
             QStringLiteral("Battle.net.exe"));
    // With a manifest name the Steam key resolves to the real title.
    QCOMPARE(resolveApplicationName(QStringLiteral("steam_app_620"), executable,
                                    QStringLiteral("steam_app_620"),
                                    QStringLiteral("Portal 2")),
             QStringLiteral("Portal 2"));
}

void ForeignWindowIdentityTests::theLastWindowsExecutableWinsOverLauncherShims()
{
    // A Proton command line begins with shims and names the real program after
    // them, so argv[0] is exactly the wrong answer.
    const auto cmdline = commandLine({"/usr/bin/umu-run", "waitforexitandrun",
                                      R"(Z:\home\user\game\Launcher.exe)",
                                      R"(D:\Games\RealGame.exe)", "-windowed"});
    QCOMPARE(executableFromCommandLine(cmdline), QStringLiteral("RealGame.exe"));

    // Forward slashes and mixed separators resolve the same way.
    QCOMPARE(executableFromCommandLine(commandLine({"wine", "/opt/app/Thing.EXE"})),
             QStringLiteral("Thing.EXE"));
}

void ForeignWindowIdentityTests::nativeClientsFallBackToArgvZero()
{
    QCOMPARE(executableFromCommandLine(commandLine({"/usr/bin/xterm", "-e", "top"})),
             QStringLiteral("xterm"));
    // With no usable executable at all the reported class survives untouched,
    // including when it is empty - exactly the previous behavior.
    QCOMPARE(resolveApplicationId(QString(), QStringLiteral("steam_app_0"), QString()),
             QStringLiteral("steam_app_0"));
    QCOMPARE(resolveApplicationId(QString(), QString(), QString()), QString());
}

void ForeignWindowIdentityTests::hostileOrAbsentCommandLinesYieldNothing()
{
    QCOMPARE(executableFromCommandLine({}), QString());
    QCOMPARE(executableFromCommandLine(commandLine({"", "", ""})), QString());
    // Oversized input is refused rather than scanned.
    QCOMPARE(executableFromCommandLine(QByteArray(9000, 'a')), QString());
    // Control characters would reach presentation.
    QCOMPARE(executableFromCommandLine(commandLine({"bad\x01name.exe"})), QString());
    // An over-long basename is refused.
    QCOMPARE(executableFromCommandLine(
                 commandLine({QByteArray(200, 'x') + ".exe"})),
             QString());
    // A value that is only separators has no basename to take.
    QCOMPARE(executableFromCommandLine(commandLine({"/"})), QString());
}

QTEST_GUILESS_MAIN(ForeignWindowIdentityTests)
#include "tst_foreignwindowidentity.moc"
