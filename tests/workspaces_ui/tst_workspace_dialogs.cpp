// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/workspaces_ui/workspace_dialogs.h"

#include <QApplication>
#include <QComboBox>
#include <QColorDialog>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace QindaQt;
using namespace QindaQt::Workspaces;
using namespace QindaQt::WorkspacesUi;

namespace {
Workspace savedWorkspace(bool duplicateApplications = false) {
  Workspace workspace;
  workspace.id = QStringLiteral("writing");
  workspace.name = QStringLiteral("Writing");
  workspace.color = QStringLiteral("#A4503A");
  const auto root = Core::LayoutNode::makeSplit(
      QStringLiteral("split"), Core::SplitOrientation::Horizontal, 0.5,
      Core::LayoutNode::makeLeaf(QStringLiteral("left"), QStringLiteral("draft")),
      Core::LayoutNode::makeLeaf(QStringLiteral("right"), QStringLiteral("notes")));
  if (!workspace.layout.addPage(Core::ContainerPage(QStringLiteral("page"), root)))
    qFatal("Invalid fixture");
  workspace.applicationSlots = {
      {QStringLiteral("draft"), QStringLiteral("Draft"),
       QStringLiteral("org.qindaqt.Editor"), {QStringLiteral("file:///draft")}},
      {QStringLiteral("notes"), QStringLiteral("Notes"),
       duplicateApplications ? QStringLiteral("org.qindaqt.Editor")
                             : QStringLiteral("org.qindaqt.Terminal"), {}}};
  return workspace;
}

class FakePort final : public WorkspaceUiPort {
public:
  std::optional<CurrentContainer> current;
  QList<WorkspaceWindow> windows;
  QSet<QString> installed;
  bool restoreSucceeds = true;
  int launched = 0;
  std::optional<Workspace> restoredWorkspace;
  std::optional<Core::WindowContainer> restoredLayout;

  std::optional<CurrentContainer> currentContainer(QString *error) override {
    if (!current && error)
      *error = QStringLiteral("No active container");
    return current;
  }
  QList<WorkspaceWindow> availableWindows(QString *) override { return windows; }
  std::optional<InstalledApplication>
  installedApplication(const QString &id) const override {
    if (!installed.contains(id))
      return std::nullopt;
    return InstalledApplication{id, id == QStringLiteral("org.qindaqt.Editor")
                                        ? QStringLiteral("QindaQt Editor")
                                        : QStringLiteral("QindaQt Terminal")};
  }
  bool launchApplication(const QString &, const QStringList &, QString *) override {
    ++launched;
    return true;
  }
  bool restore(const Workspace &workspace, const Core::WindowContainer &layout,
               QString *error) override {
    if (!restoreSucceeds) {
      if (error)
        *error = QStringLiteral("Window inventory changed");
      return false;
    }
    restoredWorkspace = workspace;
    restoredLayout = layout;
    return true;
  }
};

QDialog *activeDialog()
{
  return qobject_cast<QDialog *>(QApplication::activeModalWidget());
}
} // namespace

class WorkspaceDialogsTest final : public QObject {
  Q_OBJECT
private slots:
  void savesCurrentContainerThroughCapture() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto fixture = savedWorkspace();
    FakePort port;
    port.current = CurrentContainer{fixture.layout,
                                    {{QStringLiteral("live-draft"), fixture.applicationSlots[0]},
                                     {QStringLiteral("live-notes"), fixture.applicationSlots[1]}},
                                    QStringLiteral("Drafting"), QStringLiteral("#A4503A"), {}};
    auto plan = assignWindows(fixture, {{QStringLiteral("live-draft"),
                                         QStringLiteral("org.qindaqt.Editor"), true},
                                        {QStringLiteral("live-notes"),
                                         QStringLiteral("org.qindaqt.Terminal"), true}});
    const auto live = instantiate(fixture, plan, QStringLiteral("live-container"));
    QVERIFY(live.has_value());
    port.current->layout = *live;
    WorkspaceLibraryDialog dialog(directory.path(), port);
    dialog.show();
    QTest::mouseClick(dialog.findChild<QPushButton *>(QStringLiteral("saveCurrent")),
                      Qt::LeftButton);
    QTRY_VERIFY(activeDialog());
    auto *saveDialog = activeDialog();
    auto *nameEdit = saveDialog->findChild<QLineEdit *>(QStringLiteral("workspaceName"));
    auto *choose = saveDialog->findChild<QPushButton *>(QStringLiteral("chooseColor"));
    auto *save = saveDialog->findChild<QPushButton *>(QStringLiteral("confirmSave"));
    QVERIFY(nameEdit);
    QVERIFY(choose);
    QVERIFY(save);
    nameEdit->setText(QStringLiteral("Essay"));
    QTest::mouseClick(choose, Qt::LeftButton);
    QTRY_VERIFY(qobject_cast<QColorDialog *>(QApplication::activeModalWidget()));
    auto *picker = qobject_cast<QColorDialog *>(QApplication::activeModalWidget());
    picker->setCurrentColor(QColor(QStringLiteral("#A4503A")));
    picker->accept();
    QTRY_VERIFY(activeDialog());
    QCOMPARE(activeDialog(), saveDialog);
    QTest::mouseClick(save, Qt::LeftButton);
    WorkspaceStore store(directory.path());
    QTRY_COMPARE(store.ids().size(), 1);
    const auto written = store.load(store.ids().first());
    QVERIFY(written.has_value());
    QCOMPARE(written->name, QStringLiteral("Essay"));
    QCOMPARE(written->color, QStringLiteral("#A4503A"));
    QVERIFY(written->layout.findWindow(QStringLiteral("draft")));
  }

  void reportsDamagedSavedDocumentsWhileKeepingValidRows() {
    QTemporaryDir directory;
    const auto workspace = savedWorkspace();
    WorkspaceStore store(directory.path());
    QVERIFY(store.save(workspace));
    QFile damaged(directory.filePath(QStringLiteral("damaged.json")));
    QVERIFY(damaged.open(QIODevice::WriteOnly));
    QCOMPARE(damaged.write("{broken"), qint64(7));
    damaged.close();
    FakePort port;
    WorkspaceLibraryDialog dialog(directory.path(), port);
    QCOMPARE(dialog.findChild<QListWidget *>(QStringLiteral("savedWorkspaces"))->count(), 1);
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("workspaceResult"))
                ->text()
                .contains(QStringLiteral("could not be read")));
  }

  void explicitlyRestoresDuplicateWindowsAndForwardsWorkspaceIdentity() {
    QTemporaryDir directory;
    const auto workspace = savedWorkspace(true);
    QVERIFY(WorkspaceStore(directory.path()).save(workspace));
    FakePort port;
    port.installed.insert(QStringLiteral("org.qindaqt.Editor"));
    port.windows = {{ {QStringLiteral("terminal-one"), QStringLiteral("org.qindaqt.Editor"), true},
                      QStringLiteral("Terminal — build")},
                    {{QStringLiteral("terminal-two"), QStringLiteral("org.qindaqt.Editor"), true},
                     QStringLiteral("Terminal — logs")}};
    WorkspaceLibraryDialog dialog(directory.path(), port);
    dialog.show();
    QTest::mouseClick(dialog.findChild<QPushButton *>(QStringLiteral("reopenSelected")),
                      Qt::LeftButton);
    QTRY_VERIFY(activeDialog());
    auto *reopen = activeDialog();
    const auto boxes = reopen->findChildren<QComboBox *>();
    QCOMPARE(boxes.size(), 2);
    auto *draft = reopen->findChild<QComboBox *>(QStringLiteral("slot_draft"));
    auto *notes = reopen->findChild<QComboBox *>(QStringLiteral("slot_notes"));
    QVERIFY(draft);
    QVERIFY(notes);
    bool displayNameShown = false;
    for (const auto *label : reopen->findChildren<QLabel *>()) {
      if (label->text() == QStringLiteral("Draft — QindaQt Editor"))
        displayNameShown = true;
    }
    QVERIFY(displayNameShown);
    draft->setCurrentIndex(draft->findData(QStringLiteral("terminal-one")));
    QTest::mouseClick(reopen->findChild<QPushButton *>(QStringLiteral("refreshWindows")),
                      Qt::LeftButton);
    draft = reopen->findChild<QComboBox *>(QStringLiteral("slot_draft"));
    notes = reopen->findChild<QComboBox *>(QStringLiteral("slot_notes"));
    QVERIFY(draft);
    QVERIFY(notes);
    QCOMPARE(draft->currentData().toString(), QStringLiteral("terminal-one"));
    QTest::mouseClick(reopen->findChild<QPushButton *>(QStringLiteral("launch_draft")),
                      Qt::LeftButton);
    QCOMPARE(port.launched, 1);
    dialog.reportLaunchFailure(QStringLiteral("org.qindaqt.Editor"),
                               QStringLiteral("Desktop launch failed"));
    QCOMPARE(reopen->findChild<QLabel *>(QStringLiteral("assignmentStatus"))->text(),
             QStringLiteral("Desktop launch failed"));
    notes->setCurrentIndex(notes->findData(QStringLiteral("terminal-two")));
    auto *restore = reopen->findChild<QPushButton *>(QStringLiteral("restoreWorkspace"));
    QVERIFY(restore);
    QVERIFY(restore->isEnabled());
    QTest::mouseClick(restore, Qt::LeftButton);
    QTRY_VERIFY(port.restoredWorkspace.has_value());
    QCOMPARE(port.restoredWorkspace->id, workspace.id);
    QCOMPARE(port.restoredWorkspace->color, workspace.color);
    QVERIFY(port.restoredLayout->findWindow(QStringLiteral("terminal-one")));
    QVERIFY(port.restoredLayout->findWindow(QStringLiteral("terminal-two")));
  }

  void failedRestoreKeepsSavedDocument() {
    QTemporaryDir directory;
    const auto workspace = savedWorkspace();
    WorkspaceStore store(directory.path());
    QVERIFY(store.save(workspace));
    FakePort port;
    port.restoreSucceeds = false;
    port.installed = {QStringLiteral("org.qindaqt.Editor"), QStringLiteral("org.qindaqt.Terminal")};
    port.windows = {{{QStringLiteral("editor"), QStringLiteral("org.qindaqt.Editor"), true},
                     QStringLiteral("Essay")},
                    {{QStringLiteral("terminal"), QStringLiteral("org.qindaqt.Terminal"), true},
                     QStringLiteral("Shell")}};
    WorkspaceLibraryDialog dialog(directory.path(), port);
    dialog.show();
    QTest::mouseClick(dialog.findChild<QPushButton *>(QStringLiteral("reopenSelected")),
                      Qt::LeftButton);
    QTRY_VERIFY(activeDialog());
    auto *reopen = activeDialog();
    auto *restore = reopen->findChild<QPushButton *>(QStringLiteral("restoreWorkspace"));
    QVERIFY(restore && restore->isEnabled());
    QTest::mouseClick(restore, Qt::LeftButton);
    QTest::mouseClick(reopen->findChild<QPushButton *>(QStringLiteral("cancelReopen")),
                      Qt::LeftButton);
    QTRY_VERIFY(!activeDialog());
    const auto stillSaved = store.load(workspace.id);
    QVERIFY(stillSaved.has_value());
    QCOMPARE(stillSaved->toJson(), workspace.toJson());
  }

  void parentTeardownDeletesOpenSaveAndColorDialogs() {
    QTemporaryDir directory;
    FakePort port;
    port.current = CurrentContainer{savedWorkspace().layout, {}, {}, {}, {}};
    auto library = std::make_unique<WorkspaceLibraryDialog>(directory.path(), port);
    library->show();
    QTest::mouseClick(library->findChild<QPushButton *>(QStringLiteral("saveCurrent")),
                      Qt::LeftButton);
    QTRY_VERIFY(activeDialog());
    QPointer<QDialog> saveDialog(activeDialog());
    QTest::mouseClick(saveDialog->findChild<QPushButton *>(QStringLiteral("chooseColor")),
                      Qt::LeftButton);
    QTRY_VERIFY(qobject_cast<QColorDialog *>(QApplication::activeModalWidget()));
    QPointer<QColorDialog> picker(
        qobject_cast<QColorDialog *>(QApplication::activeModalWidget()));

    library.reset();

    QTRY_VERIFY(saveDialog.isNull());
    QTRY_VERIFY(picker.isNull());
  }

  void parentTeardownDeletesOpenReopenDialog() {
    QTemporaryDir directory;
    const auto workspace = savedWorkspace();
    QVERIFY(WorkspaceStore(directory.path()).save(workspace));
    FakePort port;
    auto library = std::make_unique<WorkspaceLibraryDialog>(directory.path(), port);
    library->show();
    QTest::mouseClick(library->findChild<QPushButton *>(QStringLiteral("reopenSelected")),
                      Qt::LeftButton);
    QTRY_VERIFY(activeDialog());
    QPointer<QDialog> reopenDialog(activeDialog());

    library.reset();

    QTRY_VERIFY(reopenDialog.isNull());
  }
};

QTEST_MAIN(WorkspaceDialogsTest)
#include "tst_workspace_dialogs.moc"
