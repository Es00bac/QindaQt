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

#include <qindaqt/services/dock_items/dock_items.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/shell/desktop_controls/dock_path_port.h>
#include <qindaqt/shell/desktop_controls/places_controller.h>
#include <qindaqt/shell/global_menu/applet/globalmenuappletaccess.h>
#include <qindaqt/shell/global_menu/protocol/menu_tree.h>
#include <qindaqt/shell/task_list/applet/task_list_applet_controller.h>
#include <qindaqt/shell/workspaces/workspace_controller.h>

#include <QObject>
#include <QSet>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

#include <functional>
#include <utility>

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
        , client(transport, Shell::Launcher::LauncherPersistenceController::scopedKeys())
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
        publishValues({{Shell::Launcher::LauncherPersistenceController::pinnedKey(),
                        QVariant(pinnedIds)}});
    }

    // ADR-0265: a baseline whose dock is the given structured value.
    void publishDock(const Services::DockItems::DockItems &dock)
    {
        publishValues({{Shell::Launcher::LauncherPersistenceController::dockItemsKey(),
                        Services::DockItems::DockItems::encodeSettingsValue(dock)}});
    }

    void publishValues(const QVariantMap &values)
    {
        QVERIFY(client.start());
        transport.announceOwner();
        QTRY_VERIFY(!transport.snapshots.isEmpty());
        transport.replyLastSnapshot(Launcher::FakeSettingsTransport::snapshotWire(
            QStringLiteral("epoch-a"), revision, values));
        QTRY_VERIFY(persistence.persistenceReady());
    }

    // The dock value of the last commit (decoded), for asserting an edit.
    Services::DockItems::DockItems committedDock() const
    {
        if (transport.commits.isEmpty())
            return {};
        const QVariantMap operation =
            transport.commits.constLast().operations.constFirst().toMap();
        const auto decoded = Services::DockItems::DockItems::decodeSettingsValue(
            operation.value(QStringLiteral("value")));
        return decoded.ok() ? *decoded.items : Services::DockItems::DockItems{};
    }

    // Confirms the last dock commit the way Settings1 would: Applied, then a
    // snapshot carrying the written value.
    void settleDock()
    {
        const QString key = Shell::Launcher::LauncherPersistenceController::dockItemsKey();
        const QVariant written = transport.commits.constLast()
                                     .operations.constFirst().toMap()
                                     .value(QStringLiteral("value"));
        const qsizetype snapshotsBefore = transport.snapshots.size();
        transport.replyLastCommit(Launcher::FakeSettingsTransport::commitWire(
            Services::SettingsProtocol::SettingsWireStatus::Applied,
            QStringLiteral("epoch-a"), revision, revision + 1, {{key, written}}));
        ++revision;
        QTRY_COMPARE(transport.snapshots.size(), snapshotsBefore + 1);
        transport.replyLastSnapshot(Launcher::FakeSettingsTransport::snapshotWire(
            QStringLiteral("epoch-a"), revision, {{key, written}}));
        QTRY_VERIFY(persistence.persistenceReady() && !persistence.writeInFlight());
    }

    quint64 revision = 0;
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

// ADR-0265: the dock's filesystem and File Manager seam, recorded. Paths
// are classified from the two sets; nothing touches the real filesystem.
class RecordingDockPaths final : public Shell::DesktopControls::DockPathPort {
public:
    PathKind classify(const QString &absolutePath) const override
    {
        if (directories.contains(absolutePath))
            return PathKind::Directory;
        return files.contains(absolutePath) ? PathKind::File : PathKind::Missing;
    }
    Shell::DesktopControls::FolderOpener::Result openFolder(const QString &absolutePath) override
    {
        openedFolders.append(absolutePath);
        return nextResult;
    }
    Shell::DesktopControls::FolderOpener::Result openFile(const QString &absolutePath) override
    {
        openedFiles.append(absolutePath);
        return nextResult;
    }
    QVariantList listFolder(const QString &absolutePath, int limit,
                            QString *diagnostic) const override
    {
        listed.append(absolutePath);
        if (diagnostic != nullptr)
            diagnostic->clear();
        return listing.mid(0, limit);
    }
    Shell::DesktopControls::FolderOpener::Result openListedEntry(const QVariantMap &entry) override
    {
        openedEntries.append(entry);
        return nextResult;
    }
    QString trashFilesDirectory() const override { return trash; }
    bool emptyTrash(std::function<void(bool, const QString &)> finished,
                    QString *diagnostic) override
    {
        ++emptyTrashCalls;
        if (!trashStarts) {
            if (diagnostic != nullptr)
                *diagnostic = QStringLiteral("trash refused");
            return false;
        }
        trashFinished = std::move(finished);
        return true;
    }

    QSet<QString> directories;
    QSet<QString> files;
    QString trash = QStringLiteral("/home/fixture/.local/share/Trash/files");
    QVariantList listing;
    mutable QStringList listed;
    QStringList openedFolders;
    QStringList openedFiles;
    QVariantList openedEntries;
    Shell::DesktopControls::FolderOpener::Result nextResult{true, {}};
    int emptyTrashCalls = 0;
    bool trashStarts = true;
    std::function<void(bool, const QString &)> trashFinished;
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
