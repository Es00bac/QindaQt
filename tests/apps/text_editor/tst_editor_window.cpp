// SPDX-License-Identifier: GPL-3.0-or-later
#include "document/local_document_store.h"
#include "ui/editor_appearance.h"
#include "ui/editor_window.h"

#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/themes/theme_loader.h"

#include <QAccessible>
#include <QAccessibleAnnouncementEvent>
#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTextCursor>

#include <memory>

using namespace QindaQt::Apps::TextEditor;

namespace {

struct AnnouncementRecord final {
  QString message;
  QAccessible::AnnouncementPoliteness politeness;
};

QVector<AnnouncementRecord> *capturedAnnouncements = nullptr;

void captureAccessibilityEvent(QAccessibleEvent *event) {
  if (!capturedAnnouncements || !event ||
      event->type() != QAccessible::Announcement) {
    return;
  }
  const auto *announcement = static_cast<QAccessibleAnnouncementEvent *>(event);
  capturedAnnouncements->append(
      {announcement->message(), announcement->politeness()});
}

class AccessibilityAnnouncementCapture final {
public:
  explicit AccessibilityAnnouncementCapture(
      QVector<AnnouncementRecord> &records)
      : m_previous(
            QAccessible::installUpdateHandler(captureAccessibilityEvent)) {
    capturedAnnouncements = &records;
  }

  ~AccessibilityAnnouncementCapture() {
    capturedAnnouncements = nullptr;
    QAccessible::installUpdateHandler(m_previous);
  }

private:
  QAccessible::UpdateHandler m_previous = nullptr;
};

} // namespace

class EditorWindowTest final : public QObject {
  Q_OBJECT

private:
  [[nodiscard]] static EditorAppearance appearance();

private slots:
  void appearanceTracksAllBuiltinThemes_data();
  void appearanceTracksAllBuiltinThemes();
  void standardActionsAndAccessibility();
  void editingPublishesDirtyState();
  void externalChangeShowsNonDestructiveBanner();
  void announcementsFollowExternalTransitionsOnly();
  void hidingBannerRestoresEditorFocus();
  void firstFrameSignalIsOneShot();
};

EditorAppearance EditorWindowTest::appearance() {
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

void EditorWindowTest::appearanceTracksAllBuiltinThemes_data() {
  QTest::addColumn<QString>("themeId");
  for (const QString &themeId : {
           QStringLiteral("qinda-dark"),
           QStringLiteral("qinda-light"),
           QStringLiteral("qinda-dusk"),
           QStringLiteral("qinda-macos"),
           QStringLiteral("qinda-high-contrast"),
       }) {
    QTest::newRow(qPrintable(themeId)) << themeId;
  }
}

void EditorWindowTest::appearanceTracksAllBuiltinThemes() {
  QFETCH(QString, themeId);
  const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/") + themeId +
      QStringLiteral(".json"));
  QVERIFY2(theme.ok, qPrintable(theme.error));

  QindaQt::DesignTokens::AccessibilityInputs inputs;
  inputs.highContrast = theme.theme.variant == QStringLiteral("high-contrast");
  const auto expected =
      QindaQt::DesignTokens::DesignTokenDeriver::derive(theme.theme, inputs);
  QVERIFY2(expected.ok(), qPrintable(expected.diagnostic));
  const auto result = EditorAppearanceAdapter::fromTheme(theme.theme);
  QVERIFY2(result.ok(), qPrintable(result.diagnostic));

  QCOMPARE(result.appearance->sourceThemeId, themeId);
  QCOMPARE(result.appearance->focusRing, expected.tokens->focusRing());
  QCOMPARE(result.appearance->warningBackground,
           expected.tokens->status().warning.background);
  QCOMPARE(result.appearance->warningForeground,
           expected.tokens->status().warning.foreground);
  QCOMPARE(result.appearance->dangerBackground,
           expected.tokens->danger().defaultColor);
  QCOMPARE(result.appearance->dangerForeground,
           expected.tokens->danger().foreground);
  QCOMPARE(expected.tokens->inputs().highContrast,
           theme.theme.variant == QStringLiteral("high-contrast"));
}

void EditorWindowTest::standardActionsAndAccessibility() {
  const EditorAppearance style = appearance();
  QCOMPARE(style.sourceThemeId, QStringLiteral("qinda-dark"));
  EditorWindow window(std::make_unique<LocalDocumentStore>(), style);
  QCOMPARE(window.palette().color(QPalette::Window),
           style.palette.color(QPalette::Window));
  QCOMPARE(window.editor()->font().family(), style.editorFont.family());
  const QList<QPair<QString, QKeySequence::StandardKey>> expectedActions{
      {QStringLiteral("fileNewAction"), QKeySequence::New},
      {QStringLiteral("fileOpenAction"), QKeySequence::Open},
      {QStringLiteral("fileSaveAction"), QKeySequence::Save},
      {QStringLiteral("fileSaveAsAction"), QKeySequence::SaveAs},
      {QStringLiteral("fileQuitAction"), QKeySequence::Quit},
      {QStringLiteral("editUndoAction"), QKeySequence::Undo},
      {QStringLiteral("editRedoAction"), QKeySequence::Redo},
      {QStringLiteral("editCutAction"), QKeySequence::Cut},
      {QStringLiteral("editCopyAction"), QKeySequence::Copy},
      {QStringLiteral("editPasteAction"), QKeySequence::Paste},
      {QStringLiteral("editSelectAllAction"), QKeySequence::SelectAll},
  };
  for (const auto &[name, standardKey] : expectedActions) {
    const auto *action = window.findChild<QAction *>(name);
    QVERIFY2(action != nullptr, qPrintable(name));
    QCOMPARE(action->shortcut(), QKeySequence(standardKey));
    QCOMPARE(action->shortcutContext(), Qt::WindowShortcut);
  }
  QVERIFY(window.findChild<QMenu *>(QStringLiteral("fileMenu")) != nullptr);
  QVERIFY(window.findChild<QMenu *>(QStringLiteral("editMenu")) != nullptr);
  QCOMPARE(window.editor()->accessibleName(), QStringLiteral("Document text"));
  QVERIFY(window.editor()->accessibleDescription().contains(
      QStringLiteral("UTF-8")));
  QCOMPARE(window.editor()->focusPolicy(), Qt::StrongFocus);
  QVERIFY(!window.findChild<QWidget *>(QStringLiteral("externalChangeBanner"))
               ->accessibleName()
               .isEmpty());
  QVERIFY(
      !window.findChild<QPushButton *>(QStringLiteral("reloadExternalAction"))
           ->accessibleName()
           .isEmpty());
  QVERIFY(
      !window.findChild<QPushButton *>(QStringLiteral("saveAsExternalAction"))
           ->accessibleName()
           .isEmpty());
}

void EditorWindowTest::editingPublishesDirtyState() {
  EditorWindow window(std::make_unique<LocalDocumentStore>(), appearance());
  window.show();
  window.editor()->setFocus();
  QTest::keyClicks(window.editor(), QStringLiteral("hello"));
  QTRY_VERIFY(window.controller()->state().isDirty());
  QCOMPARE(window.controller()->state().text(), QStringLiteral("hello"));
  QVERIFY(window.isWindowModified());
  QVERIFY(window.windowTitle().contains(QStringLiteral("Untitled")));
  QVERIFY(window.findChild<QAction *>(QStringLiteral("fileSaveAction"))
              ->isEnabled());

  QTextCursor cursor = window.editor()->textCursor();
  cursor.select(QTextCursor::Document);
  cursor.removeSelectedText();
  QTRY_VERIFY(!window.controller()->state().isDirty());
  QCOMPARE(window.controller()->state().text(), QString());

  window.editor()->insertPlainText(QStringLiteral("héllo\n☃"));
  QCOMPARE(window.controller()->state().text(), QStringLiteral("héllo\n☃"));
  QVERIFY(window.controller()->state().isDirty());
}

void EditorWindowTest::externalChangeShowsNonDestructiveBanner() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("watched.txt"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  QCOMPARE(file.write("baseline"), qint64(8));
  file.close();

  EditorWindow window(std::make_unique<LocalDocumentStore>(), appearance());
  QVERIFY(window.controller()->openPath(path).ok());
  file.setFileName(path);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  QCOMPARE(file.write("outside"), qint64(7));
  file.close();
  window.controller()->refreshExternalState();

  const auto *banner =
      window.findChild<QWidget *>(QStringLiteral("externalChangeBanner"));
  const auto *message =
      window.findChild<QLabel *>(QStringLiteral("externalChangeMessage"));
  QVERIFY(banner != nullptr);
  QVERIFY(!banner->isHidden());
  QVERIFY(message->text().contains(QStringLiteral("changed outside")));
  QVERIFY(message->text().startsWith(QStringLiteral("Warning:")));
  QVERIFY(banner->styleSheet().contains(
      appearance().warningBackground.name(QColor::HexArgb)));
  QCOMPARE(window.statusBar()->currentMessage(),
           QStringLiteral("File changed outside the editor"));
  QCOMPARE(window.controller()->state().text(), QStringLiteral("baseline"));

  QVERIFY(QFile::remove(path));
  window.controller()->refreshExternalState();
  QVERIFY(message->text().contains(QStringLiteral("was removed")));
  QVERIFY(message->text().startsWith(QStringLiteral("Error:")));
  QVERIFY(banner->styleSheet().contains(
      appearance().dangerBackground.name(QColor::HexArgb)));
  QCOMPARE(window.statusBar()->currentMessage(),
           QStringLiteral("File was removed outside the editor"));

  QVERIFY(QDir().mkdir(path));
  window.controller()->refreshExternalState();
  QVERIFY(message->text().contains(QStringLiteral("can no longer be checked")));
  QVERIFY(banner->styleSheet().contains(
      appearance().dangerBackground.name(QColor::HexArgb)));
  QCOMPARE(window.statusBar()->currentMessage(),
           QStringLiteral("File can no longer be checked"));
}

void EditorWindowTest::announcementsFollowExternalTransitionsOnly() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("announced.txt"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  QCOMPARE(file.write("baseline"), qint64(8));
  file.close();

  EditorWindow window(std::make_unique<LocalDocumentStore>(), appearance());
  QVERIFY(window.controller()->openPath(path).ok());
  QVector<AnnouncementRecord> announcements;
  AccessibilityAnnouncementCapture capture(announcements);

  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  QCOMPARE(file.write("outside"), qint64(7));
  file.close();
  window.controller()->refreshExternalState();
  QCOMPARE(announcements.size(), 1);
  QVERIFY(announcements.constFirst().message.contains(
      QStringLiteral("changed outside")));
  QCOMPARE(announcements.constFirst().politeness,
           QAccessible::AnnouncementPoliteness::Assertive);

  window.editor()->insertPlainText(QStringLiteral(" local"));
  QCOMPARE(announcements.size(), 1);
  window.controller()->refreshExternalState();
  QCOMPARE(announcements.size(), 1);

  QVERIFY(QFile::remove(path));
  window.controller()->refreshExternalState();
  QCOMPARE(announcements.size(), 2);
  QVERIFY(announcements.constLast().message.contains(
      QStringLiteral("was removed")));
}

void EditorWindowTest::hidingBannerRestoresEditorFocus() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("focused.txt"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  QCOMPARE(file.write("baseline"), qint64(8));
  file.close();

  EditorWindow window(std::make_unique<LocalDocumentStore>(), appearance());
  window.show();
  QVERIFY(window.controller()->openPath(path).ok());
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  QCOMPARE(file.write("outside"), qint64(7));
  file.close();
  window.controller()->refreshExternalState();

  auto *saveAsButton =
      window.findChild<QPushButton *>(QStringLiteral("saveAsExternalAction"));
  QVERIFY(saveAsButton != nullptr);
  saveAsButton->setFocus(Qt::TabFocusReason);
  QTRY_VERIFY(saveAsButton->hasFocus());

  const QString recovery = directory.filePath(QStringLiteral("recovered.txt"));
  QVERIFY(window.controller()->saveAs(recovery, false).ok());
  QTRY_VERIFY(window.editor()->hasFocus());
}

void EditorWindowTest::firstFrameSignalIsOneShot() {
  EditorWindow window(std::make_unique<LocalDocumentStore>(), appearance());
  QSignalSpy firstFrame(&window, &EditorWindow::firstFramePainted);
  window.show();
  QTRY_COMPARE(firstFrame.count(), 1);
  window.repaint();
  QCoreApplication::processEvents();
  QCOMPARE(firstFrame.count(), 1);
}

QTEST_MAIN(EditorWindowTest)
#include "tst_editor_window.moc"
