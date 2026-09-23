// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/session_autostart/autostart_catalog.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

using namespace QindaQt::SessionAutostart;

namespace {
void writeFile(const QString &path, const QString &text)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream(&file) << text;
}

ScanOptions options(const QTemporaryDir &root)
{
    ScanOptions value;
    value.userDirectory = root.filePath(QStringLiteral("user/autostart"));
    value.systemDirectories = {root.filePath(QStringLiteral("system/autostart"))};
    value.desktops = {QStringLiteral("QindaQt")};
    value.executableDirectories = {root.filePath(QStringLiteral("bin"))};
    return value;
}

void executable(const QTemporaryDir &root, const QString &name)
{
    const QString path = root.filePath(QStringLiteral("bin/") + name);
    writeFile(path, QStringLiteral("#!/bin/sh\nexit 0\n"));
    QFile file(path);
    QVERIFY(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                | QFileDevice::ExeOwner));
}
}

class AutostartCatalogTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void disabledWinnerMasksSystemAndDoesNotLaunch();
    void desktopTryExecAndUnsupportedLaunchFormsExplainIneligibility();
    void boundedExecExpansionProducesArgvWithoutShell();
    void escapedScalarValuesDecodeBeforeFieldCodeAndTryExec();
    void malformedScalarEscapeExplainsIneligibility();
    void terminalAndWorkingDirectoryFollowThePublicPlan();
    void environmentResolvesXdgRootsWithoutAmbientFallback();
};

void AutostartCatalogTest::disabledWinnerMasksSystemAndDoesNotLaunch()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto config = options(root);
    QVERIFY(QDir().mkpath(config.userDirectory));
    QVERIFY(QDir().mkpath(config.systemDirectories.constFirst()));
    QVERIFY(QDir().mkpath(config.executableDirectories.constFirst()));
    executable(root, QStringLiteral("app"));
    writeFile(QDir(config.systemDirectories.constFirst()).filePath(QStringLiteral("same.desktop")),
              QStringLiteral("[Desktop Entry]\nType=Application\nName=System\nExec=app\n"));
    writeFile(QDir(config.userDirectory).filePath(QStringLiteral("same.desktop")),
              QStringLiteral("[Desktop Entry]\nType=Application\nName=User\nExec=app\nHidden=true\n"));
    const auto entries = scan(config);
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.constFirst().name, QStringLiteral("User"));
    QVERIFY(!entries.constFirst().enabled);
    QVERIFY(!entries.constFirst().eligible);
    QVERIFY(!entries.constFirst().ineligibilityReason.isEmpty());
}

void AutostartCatalogTest::desktopTryExecAndUnsupportedLaunchFormsExplainIneligibility()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto config = options(root);
    QVERIFY(QDir().mkpath(config.userDirectory));
    QVERIFY(QDir().mkpath(config.executableDirectories.constFirst()));
    executable(root, QStringLiteral("app"));
    const auto put = [&config](const QString &id, const QString &tail) {
        writeFile(QDir(config.userDirectory).filePath(id + QStringLiteral(".desktop")),
                  QStringLiteral("[Desktop Entry]\nType=Application\nName=%1\nExec=app\n%2\n")
                      .arg(id, tail));
    };
    put(QStringLiteral("foreign"), QStringLiteral("OnlyShowIn=GNOME;"));
    put(QStringLiteral("blocked"), QStringLiteral("NotShowIn=QindaQt;"));
    put(QStringLiteral("missing"), QStringLiteral("TryExec=not-installed"));
    put(QStringLiteral("dbus"), QStringLiteral("DBusActivatable=true"));
    put(QStringLiteral("phase"), QStringLiteral("X-GNOME-Autostart-Phase=Initialization"));
    put(QStringLiteral("invalid"), QStringLiteral("Hidden=maybe"));
    put(QStringLiteral("conflict"), QStringLiteral("OnlyShowIn=QindaQt;\nNotShowIn=GNOME;"));
    put(QStringLiteral("valid"), QStringLiteral("OnlyShowIn=QindaQt;GNOME;\nTryExec=app"));
    const auto entries = scan(config);
    QCOMPARE(entries.size(), 8);
    for (const auto &entry : entries) {
        if (entry.id == QLatin1String("valid")) {
            QVERIFY(entry.eligible);
            QCOMPARE(entry.program, root.filePath(QStringLiteral("bin/app")));
        } else {
            QVERIFY2(!entry.eligible, qPrintable(entry.id));
            QVERIFY2(!entry.ineligibilityReason.isEmpty(), qPrintable(entry.id));
        }
    }
}

void AutostartCatalogTest::boundedExecExpansionProducesArgvWithoutShell()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto config = options(root);
    QVERIFY(QDir().mkpath(config.userDirectory));
    QVERIFY(QDir().mkpath(config.executableDirectories.constFirst()));
    executable(root, QStringLiteral("app"));
    const QString app = root.filePath(QStringLiteral("bin/app"));
    writeFile(QDir(config.userDirectory).filePath(QStringLiteral("args.desktop")),
              QStringLiteral("[Desktop Entry]\nType=Application\nName=Arguments\n"
                             "Exec=app --name %c --literal '$(touch /tmp/never)'\n"));
    const auto entries = scan(config);
    QCOMPARE(entries.size(), 1);
    const auto &entry = entries.constFirst();
    QVERIFY(entry.eligible);
    QCOMPARE(entry.program, app);
    QVERIFY(entry.arguments.contains(QStringLiteral("Arguments")));
    // Whatever string the desktop parser admits remains one argv item; the
    // autostart catalog never invokes a shell for expansion.
    QVERIFY(entry.arguments.join(QLatin1Char(' ')).contains(QStringLiteral("touch")));
}

void AutostartCatalogTest::escapedScalarValuesDecodeBeforeFieldCodeAndTryExec()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto config = options(root);
    QVERIFY(QDir().mkpath(config.userDirectory));
    QVERIFY(QDir().mkpath(config.executableDirectories.constFirst()));
    executable(root, QStringLiteral("app"));
    executable(root, QStringLiteral("helper app"));
    writeFile(QDir(config.userDirectory).filePath(QStringLiteral("escaped.desktop")),
              QStringLiteral("[Desktop Entry]\nType=Application\nName=Hello\\sWorld\n"
                             "Comment=Line\\nTwo\\\\Backslash\nIcon=folder\\sicon\n"
                             "TryExec=helper\\sapp\nExec=app %c %i\n"));
    const auto entries = scan(config);
    QCOMPARE(entries.size(), 1);
    const auto &entry = entries.constFirst();
    QVERIFY(entry.eligible);
    QCOMPARE(entry.name, QStringLiteral("Hello World"));
    QCOMPARE(entry.comment, QStringLiteral("Line\nTwo\\Backslash"));
    QCOMPARE(entry.iconName, QStringLiteral("folder icon"));
    QCOMPARE(entry.program, root.filePath(QStringLiteral("bin/app")));
    QCOMPARE(entry.arguments,
             QStringList({QStringLiteral("Hello World"), QStringLiteral("--icon"),
                          QStringLiteral("folder icon")}));
}

void AutostartCatalogTest::malformedScalarEscapeExplainsIneligibility()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto config = options(root);
    QVERIFY(QDir().mkpath(config.userDirectory));
    QVERIFY(QDir().mkpath(config.executableDirectories.constFirst()));
    executable(root, QStringLiteral("app"));
    const auto put = [&config](const QString &id, const QString &key) {
        writeFile(QDir(config.userDirectory).filePath(id + QStringLiteral(".desktop")),
                  QStringLiteral("[Desktop Entry]\nType=Application\nName=Visible\n"
                                 "Exec=app\n%1=bad\\q\n").arg(key));
    };
    put(QStringLiteral("name"), QStringLiteral("Name"));
    put(QStringLiteral("comment"), QStringLiteral("Comment"));
    put(QStringLiteral("icon"), QStringLiteral("Icon"));
    put(QStringLiteral("tryexec"), QStringLiteral("TryExec"));
    const auto entries = scan(config);
    QCOMPARE(entries.size(), 4);
    for (const auto &entry : entries) {
        QVERIFY2(!entry.eligible, qPrintable(entry.id));
        QCOMPARE(entry.ineligibilityReason, QStringLiteral("Invalid desktop entry string escape"));
    }
}

void AutostartCatalogTest::terminalAndWorkingDirectoryFollowThePublicPlan()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto config = options(root);
    QVERIFY(QDir().mkpath(config.userDirectory));
    QVERIFY(QDir().mkpath(config.executableDirectories.constFirst()));
    const QString workingDirectory = root.filePath(QStringLiteral("work"));
    QVERIFY(QDir().mkpath(workingDirectory));
    executable(root, QStringLiteral("app"));
    executable(root, QStringLiteral("qqterm"));
    writeFile(QDir(config.userDirectory).filePath(QStringLiteral("terminal.desktop")),
              QStringLiteral("[Desktop Entry]\nType=Application\nName=Terminal\n"
                             "Exec=app --verbose\nTerminal=true\nPath=%1\n")
                  .arg(workingDirectory));
    const auto entries = scan(config);
    QCOMPARE(entries.size(), 1);
    const auto &entry = entries.constFirst();
    QVERIFY(entry.eligible);
    QCOMPARE(entry.program, root.filePath(QStringLiteral("bin/qqterm")));
    QCOMPARE(entry.arguments,
             QStringList({QStringLiteral("-e"),
                          root.filePath(QStringLiteral("bin/app")),
                          QStringLiteral("--verbose")}));
    QCOMPARE(entry.workingDirectory, workingDirectory);
}

void AutostartCatalogTest::environmentResolvesXdgRootsWithoutAmbientFallback()
{
    QProcessEnvironment environment;
    environment.insert(QStringLiteral("HOME"), QStringLiteral("/tmp/autostart-home"));
    environment.insert(QStringLiteral("XDG_CONFIG_DIRS"), QStringLiteral("/one:/two"));
    environment.insert(QStringLiteral("XDG_CURRENT_DESKTOP"), QStringLiteral("QindaQt:GNOME"));
    environment.insert(QStringLiteral("PATH"), QStringLiteral("/bin:/usr/bin"));
    const auto result = ScanOptions::fromEnvironment(environment);
    QCOMPARE(result.userDirectory, QStringLiteral("/tmp/autostart-home/.config/autostart"));
    QCOMPARE(result.systemDirectories,
             QStringList({QStringLiteral("/one/autostart"), QStringLiteral("/two/autostart")}));
    QCOMPARE(result.desktops,
             QStringList({QStringLiteral("QindaQt"), QStringLiteral("GNOME")}));
    QCOMPARE(result.executableDirectories,
             QStringList({QStringLiteral("/bin"), QStringLiteral("/usr/bin")}));
}

QTEST_MAIN(AutostartCatalogTest)
#include "tst_autostart_catalog.moc"
