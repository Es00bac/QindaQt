// SPDX-License-Identifier: GPL-3.0-or-later
#include "document/local_document_store.h"
#include "restore/recovery_journal_store.h"
#include "ui/editor_application.h"
#include "ui/editor_window.h"

#include <QAction>
#include <QFile>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QTest>
#include <QTextCursor>

using namespace QindaQt::Apps::TextEditor;

namespace {

DocumentStoreFactory stores() {
  return [] { return std::make_unique<LocalDocumentStore>(); };
}

struct DialogState {
  DocumentCloseDecision close = DocumentCloseDecision::Cancel;
  RecoveryDecision recovery = RecoveryDecision::Discard;
  int recoveryPrompts = 0;
};

class ScriptedDialogs final : public DocumentDialogs {
public:
  explicit ScriptedDialogs(std::shared_ptr<DialogState> state)
      : m_state(std::move(state)) {}
  DocumentCloseDecision confirmClose() override { return m_state->close; }
  bool confirmReplace() override { return false; }
  void operationError(const QString &, const QString &) override {}
  RecoveryDecision recoverUnsaved(const QString &, bool) override {
    ++m_state->recoveryPrompts;
    return m_state->recovery;
  }

private:
  std::shared_ptr<DialogState> m_state;
};

EditorApplication::DocumentDialogFactory dialogs(std::shared_ptr<DialogState> state) {
  return [state] { return std::make_unique<ScriptedDialogs>(state); };
}

class SaveChooser final : public FileSelectionAdapter {
public:
  explicit SaveChooser(QString path) : m_path(std::move(path)) {}
  void presentOpenFile(QindaQt::AppShell::ApplicationCoordinator &coordinator,
                       const QindaQt::AppShell::PortalRequest &request) override {
    (void)coordinator.resolvePortal(request.id, true,
                                    {QUrl::fromLocalFile(m_path)});
  }
  void presentSaveFile(QindaQt::AppShell::ApplicationCoordinator &coordinator,
                       const QindaQt::AppShell::PortalRequest &request) override {
    (void)coordinator.resolvePortal(request.id, true,
                                    {QUrl::fromLocalFile(m_path)});
  }

private:
  QString m_path;
};

void writeFile(const QString &path, const QByteArray &contents) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size())
    qFatal("Could not write document fixture");
}

} // namespace

class EditorRecoveryTest final : public QObject {
  Q_OBJECT

private slots:
  void journalWritesOnFirstEditThenDebounces();
  void journalClearsOnSuccessfulSave();
  void journalClearsOnDiscardCloseAndSurvivesCancel();
  void killWithoutSaveOffersRestoreThenDiscardStopsJournaling();
  void untitledSweepRestoresOrDiscards();
  void cleanSessionLeavesNoJournalResidue();
};

void EditorRecoveryTest::journalWritesOnFirstEditThenDebounces() {
  QTemporaryDir root;
  RecoveryJournalStore journal(root.filePath(QStringLiteral("recovery")));
  EditorApplication app(stores(), nullptr, nullptr, &journal, {}, false);
  QVERIFY(app.start());
  auto *editor = app.windows().first()->editor();
  // The first dirty transition journals synchronously; later churn is
  // debounced at two seconds.
  editor->insertPlainText(QStringLiteral("first"));
  QCOMPARE(journal.entries().size(), 1);
  QVERIFY(!journal.entries().first().path.has_value());
  QCOMPARE(journal.entries().first().text, QStringLiteral("first"));
  editor->insertPlainText(QStringLiteral(" second"));
  QTRY_VERIFY_WITH_TIMEOUT(journal.entries().size() == 1, 1000);
  QTRY_COMPARE_WITH_TIMEOUT(journal.entries().first().text,
                            QStringLiteral("first second"), 4000);
}

void EditorRecoveryTest::journalClearsOnSuccessfulSave() {
  QTemporaryDir root;
  RecoveryJournalStore journal(root.filePath(QStringLiteral("recovery")));
  const QString path = root.filePath(QStringLiteral("saved.txt"));
  const auto state = std::make_shared<DialogState>();
  EditorApplication app(stores(), nullptr, nullptr, &journal,
                        [path] { return std::make_unique<SaveChooser>(path); },
                        false, dialogs(state));
  QVERIFY(app.start());
  auto *window = app.windows().first();
  window->editor()->insertPlainText(QStringLiteral("kept"));
  QCOMPARE(journal.entries().size(), 1);
  window->findChild<QAction *>(QStringLiteral("fileSaveAction"))->trigger();
  QVERIFY(!window->controller()->state().isDirty());
  QCOMPARE(window->controller()->state().path(),
           DocumentController::normalizePath(path));
  // Both the untitled identity and the new path identity are clean.
  QVERIFY(journal.entries().isEmpty());
}

void EditorRecoveryTest::journalClearsOnDiscardCloseAndSurvivesCancel() {
  QTemporaryDir root;
  RecoveryJournalStore journal(root.filePath(QStringLiteral("recovery")));
  const auto state = std::make_shared<DialogState>();
  EditorApplication app(stores(), nullptr, nullptr, &journal, {}, false,
                        dialogs(state));
  QVERIFY(app.start());
  auto *window = app.windows().first();
  window->editor()->insertPlainText(QStringLiteral("abandoned"));
  QCOMPARE(journal.entries().size(), 1);
  state->close = DocumentCloseDecision::Cancel;
  QVERIFY(!window->close());
  QCOMPARE(journal.entries().size(), 1);
  state->close = DocumentCloseDecision::Discard;
  QVERIFY(window->close());
  QVERIFY(journal.entries().isEmpty());
}

void EditorRecoveryTest::killWithoutSaveOffersRestoreThenDiscardStopsJournaling() {
  QTemporaryDir root;
  RecoveryJournalStore journal(root.filePath(QStringLiteral("recovery")));
  const QString path = root.filePath(QStringLiteral("crash.txt"));
  writeFile(path, "disk truth");
  const QString key = RecoveryJournalStore::keyForPath(
      DocumentController::normalizePath(path));
  const auto state = std::make_shared<DialogState>();
  {
    EditorApplication app(stores(), nullptr, nullptr, &journal, {}, false,
                          dialogs(state));
    QVERIFY(app.start({path}));
    auto *editor = app.windows().first()->editor();
    editor->moveCursor(QTextCursor::End);
    editor->insertPlainText(QStringLiteral(" plus unsaved"));
    QVERIFY(journal.contains(key));
    QCOMPARE(state->recoveryPrompts, 0);
    // Destroyed without save or close consent: the simulated kill.
  }
  QVERIFY(journal.contains(key));

  state->recovery = RecoveryDecision::Restore;
  {
    EditorApplication app(stores(), nullptr, nullptr, &journal, {}, false,
                          dialogs(state));
    QVERIFY(app.start({path}));
    auto *window = app.windows().first();
    QCOMPARE(state->recoveryPrompts, 1);
    QCOMPARE(window->editor()->toPlainText(),
             QStringLiteral("disk truth plus unsaved"));
    QVERIFY(window->controller()->state().isDirty());
    // The journal survives while the recovered edits are still unsaved.
    QVERIFY(journal.contains(key));
  }
  QVERIFY(journal.contains(key));

  state->recovery = RecoveryDecision::Discard;
  {
    EditorApplication app(stores(), nullptr, nullptr, &journal, {}, false,
                          dialogs(state));
    QVERIFY(app.start({path}));
    auto *window = app.windows().first();
    QCOMPARE(state->recoveryPrompts, 2);
    QCOMPARE(window->editor()->toPlainText(), QStringLiteral("disk truth"));
    QVERIFY(!window->controller()->state().isDirty());
    QVERIFY(!journal.contains(key));
    // A discarded document is never re-journaled by the same window.
    window->editor()->insertPlainText(QStringLiteral(" new work"));
    QVERIFY(!journal.contains(key));
  }
  QVERIFY(journal.entries().isEmpty());
}

void EditorRecoveryTest::untitledSweepRestoresOrDiscards() {
  QTemporaryDir root;
  RecoveryJournalStore journal(root.filePath(QStringLiteral("recovery")));
  const auto state = std::make_shared<DialogState>();
  {
    EditorApplication app(stores(), nullptr, nullptr, &journal, {}, false,
                          dialogs(state));
    QVERIFY(app.start());
    app.windows().first()->editor()->insertPlainText(
        QStringLiteral("rescued draft"));
    QCOMPARE(journal.entries().size(), 1);
  }

  state->recovery = RecoveryDecision::Restore;
  {
    EditorApplication app(stores(), nullptr, nullptr, &journal, {}, false,
                          dialogs(state));
    QVERIFY(app.start());
    QCOMPARE(state->recoveryPrompts, 1);
    // The sweep added one adopted window beyond the default untitled one.
    QCOMPARE(app.windows().size(), 2);
    EditorWindow *adopted = nullptr;
    for (auto *window : app.windows()) {
      if (window->editor()->toPlainText() == QStringLiteral("rescued draft"))
        adopted = window;
    }
    QVERIFY(adopted);
    QVERIFY(adopted->controller()->state().isUntitled());
    QVERIFY(adopted->controller()->state().isDirty());
    // The orphan journal was retired only after the content was re-journaled
    // under the adopting window's own key.
    QCOMPARE(journal.entries().size(), 1);
  }

  // A Discard consent clears the orphan and adds no window.
  QVERIFY(journal.store(RecoveryJournalStore::keyForUntitled(4242), QString(),
                        QStringLiteral("orphan"))
              .ok());
  state->recovery = RecoveryDecision::Discard;
  {
    EditorApplication app(stores(), nullptr, nullptr, &journal, {}, false,
                          dialogs(state));
    QVERIFY(app.start());
    QCOMPARE(app.windows().size(), 1);
    for (const auto &entry : journal.entries()) {
      QVERIFY(entry.text != QStringLiteral("orphan"));
    }
  }
}

void EditorRecoveryTest::cleanSessionLeavesNoJournalResidue() {
  QTemporaryDir root;
  RecoveryJournalStore journal(root.filePath(QStringLiteral("recovery")));
  const QString path = root.filePath(QStringLiteral("clean.txt"));
  writeFile(path, "settled");
  {
    EditorApplication app(stores(), nullptr, nullptr, &journal, {}, false);
    QVERIFY(app.start({path}));
    auto *editor = app.windows().first()->editor();
    editor->insertPlainText(QStringLiteral(" scratch"));
    QCOMPARE(journal.entries().size(), 1);
    // Undo-to-clean is a dirty-to-clean transition: the journal retires
    // without a save.
    editor->undo();
    QVERIFY(journal.entries().isEmpty());
  }
  QVERIFY(journal.entries().isEmpty());
}

QTEST_MAIN(EditorRecoveryTest)
#include "tst_editor_recovery.moc"
