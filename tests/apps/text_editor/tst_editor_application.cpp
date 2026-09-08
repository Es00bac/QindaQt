// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_shell/file_selection_adapter.h"
#include "document/local_document_store.h"
#include "qindaqt/themes/theme_loader.h"
#include "ui/editor_application.h"
#include "ui/editor_window.h"

#include <QAction>
#include <QApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using namespace QindaQt::Apps::TextEditor;
namespace {
DocumentStoreFactory stores() {
  return [] { return std::make_unique<LocalDocumentStore>(); };
}
EditorAppearance appearance() {
  auto theme = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
  Q_ASSERT(theme.ok);
  return *EditorAppearanceAdapter::fromTheme(theme.theme).appearance;
}
void write(const QString &path, const QByteArray &contents = "text") {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly) ||
      file.write(contents) != contents.size())
    qFatal("Could not write document fixture");
}
struct DialogState {
  DocumentCloseDecision close = DocumentCloseDecision::Cancel;
  int errors = 0;
};
class ScriptedDialogs final : public DocumentDialogs {
public:
  explicit ScriptedDialogs(std::shared_ptr<DialogState> state)
      : m_state(std::move(state)) {}
  DocumentCloseDecision confirmClose() override { return m_state->close; }
  bool confirmReplace() override { return false; }
  void operationError(const QString &, const QString &) override {
    ++m_state->errors;
  }

private:
  std::shared_ptr<DialogState> m_state;
};
EditorApplication::DocumentDialogFactory
dialogs(std::shared_ptr<DialogState> state) {
  return [state] { return std::make_unique<ScriptedDialogs>(state); };
}
class SaveChooser final : public FileSelectionAdapter {
public:
  explicit SaveChooser(QString path) : m_path(std::move(path)) {}
  void
  presentOpenFile(QindaQt::AppShell::ApplicationCoordinator &coordinator,
                  const QindaQt::AppShell::PortalRequest &request) override {
    (void)coordinator.resolvePortal(request.id, true,
                                    {QUrl::fromLocalFile(m_path)});
  }
  void
  presentSaveFile(QindaQt::AppShell::ApplicationCoordinator &coordinator,
                  const QindaQt::AppShell::PortalRequest &request) override {
    (void)coordinator.resolvePortal(request.id, true,
                                    {QUrl::fromLocalFile(m_path)});
  }

private:
  QString m_path;
};
} // namespace
class EditorApplicationTest final : public QObject {
  Q_OBJECT
private slots:
  void reusesPristineWindowAndCanonicalPaths();
  void failedOpenPreservesDirtyWindow();
  void dirtyCloseCancelThenDiscardIsWindowLocal();
  void menuCommandsRemainWindowLocal();
  void saveConsentWritesOnlyClosingDocument();
  void multiFileDropCreatesOrdinaryWindows();
  void saveAsRefusesAnotherWindowPath();
  void boundsWindowInventory();
};
void EditorApplicationTest::reusesPristineWindowAndCanonicalPaths() {
  QTemporaryDir root;
  const auto one = root.filePath("one.txt"), two = root.filePath("two.txt");
  const auto alias = root.filePath("alias.txt");
  write(one);
  write(two);
  QVERIFY(QFile::link(one, alias));
  EditorApplication app(stores(), appearance(), nullptr, nullptr, {}, false);
  QVERIFY(app.start());
  auto *initial = app.windows().first();
  QVERIFY(app.openDocuments({one, two, alias}, initial));
  QCOMPARE(app.windows().size(), 2);
  QCOMPARE(app.windowForPath(one), initial);
  QCOMPARE(app.windowForPath(alias), initial);
  QCOMPARE(app.openPaths(), (QStringList{one, two}));
  for (auto *window : app.windows()) {
    QVERIFY(window->isWindow());
    QVERIFY(!window->parentWidget());
    QCOMPARE(window->findChildren<QPlainTextEdit *>().size(), 1);
  }
}
void EditorApplicationTest::failedOpenPreservesDirtyWindow() {
  QTemporaryDir root;
  const auto good = root.filePath("good.txt");
  write(good);
  EditorApplication app(stores(), appearance(), nullptr, nullptr, {}, false);
  QVERIFY(app.start());
  auto *initial = app.windows().first();
  initial->editor()->insertPlainText("keep me");
  QString diagnostic;
  QVERIFY(!app.openDocuments({root.filePath("absent.txt"), good}, initial,
                             &diagnostic));
  QVERIFY(!diagnostic.isEmpty());
  QCOMPARE(initial->editor()->toPlainText(), QStringLiteral("keep me"));
  QCOMPARE(app.windows().size(), 2);
  QCOMPARE(app.windowForPath(good)->editor()->toPlainText(),
           QStringLiteral("text"));
}
void EditorApplicationTest::dirtyCloseCancelThenDiscardIsWindowLocal() {
  const auto state = std::make_shared<DialogState>();
  EditorApplication app(stores(), appearance(), nullptr, nullptr, {}, false,
                        dialogs(state));
  QVERIFY(app.start());
  auto *one = app.windows().first();
  auto *two = app.newWindow();
  QVERIFY(two);
  one->editor()->insertPlainText("one");
  two->editor()->insertPlainText("two");
  QSignalSpy closed(one, &EditorWindow::closeAccepted);
  state->close = DocumentCloseDecision::Cancel;
  QVERIFY(!one->close());
  QCOMPARE(closed.size(), 0);
  QCOMPARE(app.windows().size(), 2);
  QCOMPARE(one->editor()->toPlainText(), QStringLiteral("one"));
  state->close = DocumentCloseDecision::Discard;
  QVERIFY(one->close());
  QCOMPARE(closed.size(), 1);
  QCOMPARE(app.windows().size(), 1);
  QCOMPARE(two->editor()->toPlainText(), QStringLiteral("two"));
  QVERIFY(two->controller()->state().isDirty());
}
void EditorApplicationTest::saveConsentWritesOnlyClosingDocument() {
  const auto state = std::make_shared<DialogState>();
  QTemporaryDir root;
  const auto path = root.filePath("saved.txt");
  EditorApplication app(
      stores(), appearance(), nullptr, nullptr,
      [path] { return std::make_unique<SaveChooser>(path); }, false,
      dialogs(state));
  QVERIFY(app.start());
  auto *one = app.windows().first();
  auto *two = app.newWindow();
  one->editor()->insertPlainText("save exactly this");
  two->editor()->insertPlainText("keep editing");
  state->close = DocumentCloseDecision::Save;
  QVERIFY(one->close());
  QCOMPARE(app.windows().size(), 1);
  QFile file(path);
  QVERIFY(file.open(QIODevice::ReadOnly));
  QCOMPARE(file.readAll(), QByteArray("save exactly this"));
  QCOMPARE(two->editor()->toPlainText(), QStringLiteral("keep editing"));
  QVERIFY(two->controller()->state().isDirty());
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QVERIFY(two->appShellCoordinator().activateAction(
      QStringLiteral("edit.select-all")));
  QCOMPARE(two->editor()->textCursor().selectedText(),
           QStringLiteral("keep editing"));
}

void EditorApplicationTest::menuCommandsRemainWindowLocal() {
  EditorApplication app(stores(), appearance(), nullptr, nullptr, {}, false);
  QVERIFY(app.start());
  auto *one = app.windows().first();
  QVERIFY(
      one->appShellCoordinator().activateAction(QStringLiteral("file.new")));
  QCOMPARE(app.windows().size(), 2);
  auto *two = app.windows().last();
  one->editor()->insertPlainText("one");
  two->editor()->insertPlainText("two");
  QVERIFY(one->appShellCoordinator().activateAction(
      QStringLiteral("edit.select-all")));
  QCOMPARE(one->editor()->textCursor().selectedText(), QStringLiteral("one"));
  QVERIFY(!two->editor()->textCursor().hasSelection());
  QVERIFY(
      two->appShellCoordinator().activateAction(QStringLiteral("edit.undo")));
  QCOMPARE(two->editor()->toPlainText(), QString());
  QCOMPARE(one->editor()->toPlainText(), QStringLiteral("one"));
  QVERIFY(
      !one->appShellCoordinator().activateAction(QStringLiteral("tabs.next")));
}
void EditorApplicationTest::multiFileDropCreatesOrdinaryWindows() {
  QTemporaryDir root;
  const auto one = root.filePath("one.txt"), two = root.filePath("two.txt");
  write(one);
  write(two);
  EditorApplication app(stores(), appearance(), nullptr, nullptr, {}, false);
  QVERIFY(app.start());
  auto *window = app.windows().first();
  QMimeData data;
  data.setUrls({QUrl::fromLocalFile(one), QUrl::fromLocalFile(two)});
  QDragEnterEvent enter(QPoint(10, 10), Qt::CopyAction, &data, Qt::LeftButton,
                        Qt::NoModifier);
  QApplication::sendEvent(window, &enter);
  QVERIFY(enter.isAccepted());
  QDropEvent drop(QPointF(10, 10), Qt::CopyAction, &data, Qt::LeftButton,
                  Qt::NoModifier);
  QApplication::sendEvent(window, &drop);
  QVERIFY(drop.isAccepted());
  QCOMPARE(app.windows().size(), 2);
  QCOMPARE(app.openPaths(), (QStringList{one, two}));
}
void EditorApplicationTest::saveAsRefusesAnotherWindowPath() {
  const auto state = std::make_shared<DialogState>();
  QTemporaryDir root;
  const auto path = root.filePath("owned.txt");
  write(path, "disk truth");
  EditorApplication app(
      stores(), appearance(), nullptr, nullptr,
      [path] { return std::make_unique<SaveChooser>(path); }, false,
      dialogs(state));
  QVERIFY(app.start({path}));
  auto *untitled = app.newWindow();
  untitled->editor()->insertPlainText("must not overwrite");
  QVERIFY(untitled->appShellCoordinator().activateAction(
      QStringLiteral("file.save-as")));
  QCOMPARE(state->errors, 1);
  QVERIFY(untitled->controller()->state().isUntitled());
  QVERIFY(untitled->controller()->state().isDirty());
  QFile file(path);
  QVERIFY(file.open(QIODevice::ReadOnly));
  QCOMPARE(file.readAll(), QByteArray("disk truth"));
}
void EditorApplicationTest::boundsWindowInventory() {
  EditorApplication app(stores(), appearance(), nullptr, nullptr, {}, false);
  for (int i = 0; i < DocumentCollection::maximumDocuments; ++i)
    QVERIFY(app.newWindow());
  QVERIFY(!app.newWindow());
  QCOMPARE(app.windows().size(), DocumentCollection::maximumDocuments);
}
QTEST_MAIN(EditorApplicationTest)
#include "tst_editor_application.moc"
