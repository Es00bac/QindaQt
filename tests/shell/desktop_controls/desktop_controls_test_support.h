// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Shared fixtures for the desktop-controls tests. Everything is built from
// the existing applet test doubles: the launcher L1 stack with recording
// spawner/activator, the task-list T0/T1 fakes, the power/audio fake
// transports, a scripted global-menu tree, and the fake workspace transport.
// AGENT-CONTRACT: these helpers never start applications, touch the host
// session bus, or read the user's real data roots.

#include "application_scanner.h"
#include "launch_executor.h"
#include "launcher_applet_controller.h"
#include "launcher_persistence.h"
#include "launcher_runtime_test_support.h"

#include "fake_workspace_transport.h"
#include "task_list_applet_test_fakes.h"
#include "task_list_operation_test_support.h"
#include "task_list_test_support.h"

#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/shell/desktop_controls/places_controller.h>
#include <qindaqt/shell/global_menu/applet/globalmenuappletaccess.h>
#include <qindaqt/shell/global_menu/protocol/menu_tree.h>
#include <qindaqt/shell/task_list/applet/task_list_applet_controller.h>
#include <qindaqt/shell/workspaces/workspace_controller.h>

#include <QObject>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

namespace QindaQt::Tests::DesktopControls {

// Launcher L1 stack (mirrors tests/shell/launcher/tst_launcher_controller.cpp).
// Member order is construction order: root outlives scanner, scanner exists
// before the executor that borrows it.
struct LauncherStack {
    QTemporaryDir root;
    Shell::Launcher::ApplicationScanner scanner;
    Launcher::FakeSettingsTransport transport;
    Services::SettingsClient::SettingsClient client;
    Shell::Launcher::LauncherPersistenceController persistence;
    Launcher::RecordingSpawner spawner;
    Launcher::RecordingActivator activator;
    Shell::Launcher::LaunchExecutor executor;

    LauncherStack()
        : scanner({root.path()})
        , client(transport,
                 {Shell::Launcher::LauncherPersistenceController::pinnedKey(),
                  Shell::Launcher::LauncherPersistenceController::recentKey()})
        , persistence(client)
        , executor(scanner, spawner, activator)
    {
    }

    bool addEntry(const QString &file, const QString &name, const QString &exec,
                  const QString &extra = {})
    {
        return Launcher::writeDesktopFile(root.path(), file,
                                          Launcher::minimalEntry(name, exec, extra));
    }

    // Publishes a Settings1 baseline so pinned/recent identities are known.
    void publishPinned(const QStringList &pinnedIds)
    {
        QVERIFY(client.start());
        transport.announceOwner();
        QTRY_VERIFY(!transport.snapshots.isEmpty());
        transport.replyLastSnapshot(Launcher::FakeSettingsTransport::snapshotWire(
            QStringLiteral("epoch-a"), 0,
            {{Shell::Launcher::LauncherPersistenceController::pinnedKey(),
              QVariant(pinnedIds)}}));
        QTRY_VERIFY(persistence.persistenceReady());
    }
};

// Task-list T0 source + T1 authority fake + recording port.
struct TaskListStack {
    ShellTaskList::TaskListSource source;
    TaskListOperationTest::FakeOperationAuthority authority;
    TaskListAppletTest::FakeTaskListOperationPort port;

    quint64 publish(const QVector<ShellTaskList::TaskWindowFact> &facts)
    {
        const auto evaluation = source.publishGeneration(facts);
        if (!evaluation.ok()) {
            return 0;
        }
        authority.revision = source.revision();
        authority.sourceStatus = ShellTaskList::TaskListSourceStatus::Ready;
        authority.owner = QStringLiteral(":1.1");
        Q_EMIT authority.stateChanged();
        return source.revision();
    }

    static QVector<ShellTaskList::TaskWindowFact> activeEditorFacts()
    {
        auto editor = TaskListTest::standalone(QStringLiteral("w-editor"),
                                               QStringLiteral("org.qindaqt.TextEditor"));
        editor.active = true;
        auto terminal = TaskListTest::standalone(QStringLiteral("w-terminal"),
                                                 QStringLiteral("org.qindaqt.Terminal"));
        terminal.minimized = true;
        return {editor, terminal};
    }
};

inline Shell::GlobalMenu::Protocol::MenuTree menuTree(const QString &openText = QStringLiteral("Open"),
                                                      bool quitEnabled = false)
{
    using namespace Shell::GlobalMenu::Protocol;
    MenuItem open;
    open.id = QStringLiteral("fileOpen");
    open.text = openText;
    open.shortcutText = QStringLiteral("Ctrl+O");
    MenuItem quit;
    quit.id = QStringLiteral("fileQuit");
    quit.text = QStringLiteral("Quit");
    quit.enabled = quitEnabled;
    MenuItem hidden;
    hidden.id = QStringLiteral("fileHidden");
    hidden.text = QStringLiteral("Hidden");
    hidden.visible = false;
    MenuItem separator;
    separator.id = QStringLiteral("sep");
    separator.kind = MenuItemKind::Separator;
    MenuItem recent;
    recent.id = QStringLiteral("recentMenu");
    recent.kind = MenuItemKind::Submenu;
    recent.text = QStringLiteral("Recent");
    MenuItem alpha;
    alpha.id = QStringLiteral("recentAlpha");
    alpha.text = QStringLiteral("Alpha notes");
    recent.children = {alpha};
    MenuItem file;
    file.id = QStringLiteral("fileMenu");
    file.kind = MenuItemKind::Submenu;
    file.text = QStringLiteral("File");
    file.children = {open, separator, recent, hidden, quit};
    MenuTree tree;
    tree.ownerWindowId = QUuid::createUuid();
    tree.epoch = QUuid::createUuid();
    tree.revision = 1;
    tree.items = {file};
    return tree;
}

// Workspace controller over the fake transport, published ready.
struct WorkspaceStack {
    Workspaces::FakeWorkspaceTransport transport;
    Shell::Workspaces::WorkspaceController controller{&transport, {true, true}};

    void publishReady(const Shell::Workspaces::WorkspaceSnapshot &snapshot =
                          Workspaces::fixtureSnapshot())
    {
        transport.announce(QStringLiteral(":1.7"));
        transport.reply(transport.snapshotRequests.constLast(), snapshot);
    }
};

class RecordingFolderOpener final : public Shell::DesktopControls::FolderOpener {
public:
    Result open(const QString &absoluteDirectory) override
    {
        opened.append(absoluteDirectory);
        return nextResult;
    }

    QStringList opened;
    Result nextResult{true, {}};
};

class StubSessionActions final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool canLock MEMBER canLock NOTIFY availabilityChanged)
    Q_PROPERTY(bool canLogout MEMBER canLogout NOTIFY availabilityChanged)
    Q_PROPERTY(bool canSuspend MEMBER canSuspend NOTIFY availabilityChanged)
    Q_PROPERTY(bool canReboot MEMBER canReboot NOTIFY availabilityChanged)
    Q_PROPERTY(bool canPowerOff MEMBER canPowerOff NOTIFY availabilityChanged)
    Q_PROPERTY(bool pending MEMBER pending NOTIFY pendingChanged)
    Q_PROPERTY(QString feedback MEMBER feedback NOTIFY feedbackChanged)

public:
    bool canLock = true;
    bool canLogout = true;
    bool canSuspend = false;
    bool canReboot = true;
    bool canPowerOff = true;
    bool pending = false;
    QString feedback;
    QStringList requests;

    Q_INVOKABLE bool requestLock() { requests.append(QStringLiteral("lock")); return true; }
    Q_INVOKABLE bool requestLogout() { requests.append(QStringLiteral("logout")); return true; }
    Q_INVOKABLE bool requestSuspend() { requests.append(QStringLiteral("suspend")); return true; }
    Q_INVOKABLE bool requestReboot() { requests.append(QStringLiteral("reboot")); return true; }
    Q_INVOKABLE bool requestPowerOff() { requests.append(QStringLiteral("poweroff")); return true; }

Q_SIGNALS:
    void availabilityChanged();
    void pendingChanged();
    void feedbackChanged();
};

} // namespace QindaQt::Tests::DesktopControls
