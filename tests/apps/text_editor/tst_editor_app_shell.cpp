// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/editor_action_catalog.h"
#include "app_shell/fail_closed_file_selection_adapter.h"
#include "app_shell/file_selection_adapter.h"
#include "document/local_document_store.h"
#include "ui/editor_appearance.h"
#include "ui/editor_window.h"

#include "qindaqt/app_shell/application_coordinator.h"
#include "qindaqt/themes/theme_loader.h"

#include <QAction>
#include <QFile>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextCursor>
#include <QUrl>
#include <QVariantMap>

#include <memory>

using namespace QindaQt::Apps::TextEditor;
using namespace QindaQt::AppShell;

namespace {

// A minimal test double that resolves every request with one fixed path,
// simulating a user who always picks the same file without contacting any
// real host chooser.
class FixedFileSelectionAdapter final : public FileSelectionAdapter {
public:
  explicit FixedFileSelectionAdapter(QString path) : m_path(std::move(path)) {}

  void presentOpenFile(ApplicationCoordinator &coordinator,
                       const PortalRequest &request) override {
    (void)coordinator.resolvePortal(request.id, true,
                                    {QUrl::fromLocalFile(m_path)});
  }
  void presentSaveFile(ApplicationCoordinator &coordinator,
                       const PortalRequest &request) override {
    (void)coordinator.resolvePortal(request.id, true,
                                    {QUrl::fromLocalFile(m_path)});
  }

private:
  QString m_path;
};

[[nodiscard]] EditorAppearance appearance() {
  const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
  if (!theme.ok) {
    qFatal("Could not load test theme: %s", qPrintable(theme.error));
  }
  const auto result = EditorAppearanceAdapter::fromTheme(theme.theme);
  if (!result.ok()) {
    qFatal("Could not derive test appearance: %s",
           qPrintable(result.diagnostic));
  }
  return *result.appearance;
}

[[nodiscard]] QVariantMap findAction(const QVariantList &menus,
                                     const QString &menuId,
                                     const QString &actionId) {
  for (const QVariant &menu : menus) {
    const QVariantMap menuMap = menu.toMap();
    if (menuMap.value(QStringLiteral("id")).toString() != menuId) {
      continue;
    }
    for (const QVariant &action : menuMap.value(QStringLiteral("actions")).toList()) {
      const QVariantMap actionMap = action.toMap();
      if (actionMap.value(QStringLiteral("id")).toString() == actionId) {
        return actionMap;
      }
    }
  }
  return {};
}

} // namespace

class EditorAppShellTest final : public QObject {
  Q_OBJECT

private slots:
  void catalogMatchesDocumentedActionsAndValidates();
  void publishesAtomicMenuSnapshotWithSyncedInitialProjection();
  void dirtyStateProjectsFileSaveEnabled();
  void activatingKnownActionTriggersTheLocalCommand();
  void closeRoutesThroughAppShellQuitConsent();
  void fileSelectionFailsClosedWithoutARealDialog();
  void injectedAdapterResolvesAnOpenRequest();
};

void EditorAppShellTest::catalogMatchesDocumentedActionsAndValidates() {
  const QList<ActionSpec> catalog = editorActionCatalog();
  QCOMPARE(catalog.size(), 11);

  const QSet<QString> expectedIds{
      QString::fromLatin1(AppShellActionIds::FileNew),
      QString::fromLatin1(AppShellActionIds::FileOpen),
      QString::fromLatin1(AppShellActionIds::FileSave),
      QString::fromLatin1(AppShellActionIds::FileSaveAs),
      QString::fromLatin1(AppShellActionIds::FileQuit),
      QString::fromLatin1(AppShellActionIds::EditUndo),
      QString::fromLatin1(AppShellActionIds::EditRedo),
      QString::fromLatin1(AppShellActionIds::EditCut),
      QString::fromLatin1(AppShellActionIds::EditCopy),
      QString::fromLatin1(AppShellActionIds::EditPaste),
      QString::fromLatin1(AppShellActionIds::EditSelectAll),
  };
  QSet<QString> actualIds;
  for (const ActionSpec &action : catalog) {
    actualIds.insert(action.id);
    QVERIFY(!action.shortcut.isEmpty());
    QVERIFY(action.menuId == QStringLiteral("file") ||
            action.menuId == QStringLiteral("edit"));
  }
  QCOMPARE(actualIds, expectedIds);

  // The catalog must satisfy the real ActionRegistry validation, not just
  // look plausible.
  ApplicationCoordinator coordinator;
  QCOMPARE(coordinator.replaceActions(catalog).code, ErrorCode::None);
}

void EditorAppShellTest::publishesAtomicMenuSnapshotWithSyncedInitialProjection() {
  EditorWindow window(std::make_unique<LocalDocumentStore>(), appearance(),
                      std::make_unique<FailClosedFileSelectionAdapter>());
  const QVariantList menus = window.appShellCoordinator().menus();
  QCOMPARE(menus.size(), 2);

  const QVariantMap fileNew =
      findAction(menus, QStringLiteral("file"),
                QString::fromLatin1(AppShellActionIds::FileNew));
  QVERIFY(!fileNew.isEmpty());
  QVERIFY(fileNew.value(QStringLiteral("enabled")).toBool());

  const QVariantMap fileSave =
      findAction(menus, QStringLiteral("file"),
                QString::fromLatin1(AppShellActionIds::FileSave));
  QCOMPARE(fileSave.value(QStringLiteral("enabled")).toBool(),
           window.findChild<QAction *>(QStringLiteral("fileSaveAction"))
               ->isEnabled());

  const QVariantMap editUndo =
      findAction(menus, QStringLiteral("edit"),
                QString::fromLatin1(AppShellActionIds::EditUndo));
  QVERIFY(!editUndo.value(QStringLiteral("enabled")).toBool());
}

void EditorAppShellTest::dirtyStateProjectsFileSaveEnabled() {
  EditorWindow window(std::make_unique<LocalDocumentStore>(), appearance(),
                      std::make_unique<FailClosedFileSelectionAdapter>());
  window.show();
  window.editor()->setFocus();
  QTest::keyClicks(window.editor(), QStringLiteral("hello"));
  QTRY_VERIFY(window.controller()->state().isDirty());

  const QVariantMap fileSave =
      findAction(window.appShellCoordinator().menus(), QStringLiteral("file"),
                QString::fromLatin1(AppShellActionIds::FileSave));
  QVERIFY(fileSave.value(QStringLiteral("enabled")).toBool());
}

void EditorAppShellTest::activatingKnownActionTriggersTheLocalCommand() {
  EditorWindow window(std::make_unique<LocalDocumentStore>(), appearance(),
                      std::make_unique<FailClosedFileSelectionAdapter>());
  window.editor()->insertPlainText(QStringLiteral("select me"));
  QVERIFY(!window.editor()->textCursor().hasSelection());

  QVERIFY(window.appShellCoordinator().activateAction(
      QString::fromLatin1(AppShellActionIds::EditSelectAll)));
  QVERIFY(window.editor()->textCursor().hasSelection());
  QCOMPARE(window.editor()->textCursor().selectedText(),
           QStringLiteral("select me"));
}

void EditorAppShellTest::closeRoutesThroughAppShellQuitConsent() {
  EditorWindow window(std::make_unique<LocalDocumentStore>(), appearance(),
                      std::make_unique<FailClosedFileSelectionAdapter>());
  window.show();
  QSignalSpy approved(&window.appShellCoordinator(),
                      &ApplicationCoordinator::quitApproved);
  QVERIFY(!window.controller()->state().isDirty());

  window.close();

  QCOMPARE(approved.count(), 1);
  QVERIFY(!window.isVisible());
}

void EditorAppShellTest::fileSelectionFailsClosedWithoutARealDialog() {
  EditorWindow window(std::make_unique<LocalDocumentStore>(), appearance(),
                      std::make_unique<FailClosedFileSelectionAdapter>());
  QSignalSpy finished(&window.appShellCoordinator(),
                      &ApplicationCoordinator::portalFinished);

  QVERIFY(window.appShellCoordinator().activateAction(
      QString::fromLatin1(AppShellActionIds::FileOpen)));

  QCOMPARE(finished.count(), 1);
  const auto result = finished.constFirst().constFirst().value<PortalResult>();
  QVERIFY(!result.accepted);
  QCOMPARE(result.error.code, ErrorCode::Denied);
  QVERIFY(window.controller()->state().isUntitled());
}

void EditorAppShellTest::injectedAdapterResolvesAnOpenRequest() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("fixture.txt"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  QCOMPARE(file.write("picked by the fixed adapter"), qint64(27));
  file.close();

  EditorWindow window(std::make_unique<LocalDocumentStore>(), appearance(),
                      std::make_unique<FixedFileSelectionAdapter>(path));
  QVERIFY(window.appShellCoordinator().activateAction(
      QString::fromLatin1(AppShellActionIds::FileOpen)));

  QVERIFY(!window.controller()->state().isUntitled());
  QCOMPARE(window.controller()->state().text(),
           QStringLiteral("picked by the fixed adapter"));
}

QTEST_MAIN(EditorAppShellTest)
#include "tst_editor_app_shell.moc"
