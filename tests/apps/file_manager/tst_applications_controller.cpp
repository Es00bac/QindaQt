// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/applications_controller.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using QindaQt::Apps::FileManager::ApplicationsController;

namespace {

bool writeDesktop(const QString &path, const QString &text)
{
    if (!QDir().mkpath(QFileInfo(path).path())) {
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    return file.write(text.toUtf8()) >= 0;
}

QString entryText(const QString &name, const QString &exec,
                  const QString &categories, const QString &extra = {})
{
    return QStringLiteral("[Desktop Entry]\nType=Application\nName=%1\n"
                          "Exec=%2\nCategories=%3\n%4")
        .arg(name, exec, categories, extra);
}

QVariantMap rowById(const QVariantList &rows, const QString &id)
{
    for (const auto &row : rows) {
        const auto map = row.toMap();
        if (map.value(QStringLiteral("id")).toString() == id) {
            return map;
        }
    }
    return {};
}

} // namespace

class ApplicationsControllerTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void browsesFoldersAndEntries();
    void reportsLaunchabilityAndTypedLimitations();
    void launchesPlainProcessesDetached();
};

void ApplicationsControllerTests::browsesFoldersAndEntries()
{
    QTemporaryDir root;
    QVERIFY(writeDesktop(root.filePath("applications/editor.desktop"),
                         entryText("Editor", "/usr/bin/editor",
                                   QStringLiteral("Development;TextEditor;"))));
    QVERIFY(writeDesktop(root.filePath("applications/game.desktop"),
                         entryText("Game", "/usr/bin/game",
                                   QStringLiteral("Game;StrategyGame;"))));
    QVERIFY(writeDesktop(root.filePath("applications/loose.desktop"),
                         entryText("Loose", "/usr/bin/loose", {})));

    ApplicationsController controller({root.path()});
    controller.refresh();
    QVERIFY(controller.ready());
    QVERIFY(controller.breadcrumb().isEmpty());

    // Root shows only the populated main groups.
    const QVariantMap development = rowById(controller.folders(),
                                            QStringLiteral("Development"));
    QVERIFY(!development.isEmpty());
    QCOMPARE(development.value(QStringLiteral("entryCount")).toInt(), 1);
    QVERIFY(rowById(controller.folders(), QStringLiteral("Graphics")).isEmpty());

    controller.openFolder(QStringLiteral("Development"));
    QCOMPARE(controller.breadcrumb(), QStringLiteral("Development"));
    // The registered additional category becomes a nested child folder.
    QCOMPARE(controller.entries().size(), 0);
    const QVariantMap textEditors = rowById(
        controller.folders(), QStringLiteral("TextEditor"));
    QVERIFY(!textEditors.isEmpty());

    controller.openFolder(QStringLiteral("Development/TextEditor"));
    QCOMPARE(controller.entries().size(), 1);
    QVERIFY(rowById(controller.entries(), QStringLiteral("editor"))
                .value(QStringLiteral("launchable")).toBool());

    controller.openParentFolder();
    QCOMPARE(controller.breadcrumb(), QStringLiteral("Development"));
    controller.openParentFolder();
    QVERIFY(controller.breadcrumb().isEmpty());

    // Unclassified entries land in Other.
    QVERIFY(!rowById(controller.folders(), QStringLiteral("Other")).isEmpty());
}

void ApplicationsControllerTests::reportsLaunchabilityAndTypedLimitations()
{
    QTemporaryDir root;
    QVERIFY(writeDesktop(root.filePath("applications/plain.desktop"),
                         entryText("Plain", "/usr/bin/plain", {})));
    QVERIFY(writeDesktop(root.filePath("applications/terminal.desktop"),
                         entryText("TerminalApp", "/usr/bin/term",
                                   {},
                                   QStringLiteral("Terminal=true\n"))));
    QVERIFY(writeDesktop(root.filePath("applications/dbus.desktop"),
                         entryText("DbusApp", "/usr/bin/dbus",
                                   {},
                                   QStringLiteral("DBusActivatable=true\n"))));

    ApplicationsController controller({root.path()});
    controller.refresh();
    controller.openFolder(QStringLiteral("Other"));
    QCOMPARE(rowById(controller.entries(), QStringLiteral("plain"))
                 .value(QStringLiteral("launchable")).toBool(),
             true);
    const auto terminal = rowById(controller.entries(),
                                  QStringLiteral("terminal"));
    QCOMPARE(terminal.value(QStringLiteral("launchable")).toBool(), false);
    QVERIFY(terminal.value(QStringLiteral("message")).toString().contains(
        QStringLiteral("terminal")));

    // Activating a non-launchable entry records the typed limitation and
    // never spawns.
    controller.activateEntry(QStringLiteral("dbus"));
    QVERIFY(controller.lastError().contains(QStringLiteral("D-Bus")));
    controller.activateEntry(QStringLiteral("missing"));
    QVERIFY(controller.lastError().contains(QStringLiteral("installed")));
}

void ApplicationsControllerTests::launchesPlainProcessesDetached()
{
    QTemporaryDir root;
    QVERIFY(writeDesktop(root.filePath("applications/spawn.desktop"),
                         entryText("Spawn", "/usr/bin/true", {})));
    ApplicationsController controller({root.path()});
    controller.refresh();
    controller.activateEntry(QStringLiteral("spawn"));
    QVERIFY(controller.lastError().isEmpty());

    // An entry whose Exec fails to plan reports instead of crashing.
    QVERIFY(writeDesktop(root.filePath("applications/broken.desktop"),
                         QStringLiteral("[Desktop Entry]\nType=Application\n"
                                        "Name=Broken\n")));
    controller.refresh();
    controller.activateEntry(QStringLiteral("broken"));
    QVERIFY(!controller.lastError().isEmpty());
}

QTEST_GUILESS_MAIN(ApplicationsControllerTests)
#include "tst_applications_controller.moc"
