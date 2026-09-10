// SPDX-License-Identifier: GPL-3.0-or-later
#include "document/local_document_store.h"
#include "qindaqt/design_tokens/token_deriver.h"
#include "ui/document_editor.h"
#include "ui/editor_application.h"
#include "ui/editor_window.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBlock>
#include <QTextLayout>
#include <QToolBar>

using namespace QindaQt::Apps::TextEditor;
namespace {

// ADR-0116: the platform theme owns the palette. These fixtures stand in for
// dark, light, and high-contrast platform palettes; the application must
// render readably from palette roles alone, with no token authority.
QPalette fixturePalette(const QString &kind) {
  QPalette palette;
  const QColor window = kind == QStringLiteral("light") ? QColor(245, 245, 247)
      : kind == QStringLiteral("high-contrast")         ? QColor(0, 0, 0)
                                                        : QColor(30, 30, 40);
  const QColor base = kind == QStringLiteral("light") ? QColor(255, 255, 255)
      : kind == QStringLiteral("high-contrast")       ? QColor(0, 0, 0)
                                                      : QColor(20, 20, 28);
  const QColor text = kind == QStringLiteral("light") ? QColor(26, 26, 32)
                                                      : QColor(232, 230, 240);
  const QColor alternate = kind == QStringLiteral("light")
      ? QColor(236, 236, 240)
      : kind == QStringLiteral("high-contrast") ? QColor(0, 0, 0)
                                                : QColor(38, 38, 50);
  palette.setColor(QPalette::Window, window);
  palette.setColor(QPalette::WindowText, text);
  palette.setColor(QPalette::Base, base);
  palette.setColor(QPalette::AlternateBase, alternate);
  palette.setColor(QPalette::Text, text);
  palette.setColor(QPalette::Button, window);
  palette.setColor(QPalette::ButtonText, text);
  palette.setColor(QPalette::PlaceholderText, QColor(140, 140, 150));
  palette.setColor(QPalette::Highlight, QColor(70, 110, 200));
  palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
  return palette;
}

} // namespace

class EditorVisualTest final : public QObject {
  Q_OBJECT
private slots:
  void actualWindow_data() {
    QTest::addColumn<QString>("theme");
    QTest::addColumn<QSize>("size");
    for (const auto &theme :
         {QStringLiteral("dark"), QStringLiteral("light"),
          QStringLiteral("high-contrast")}) {
      QTest::newRow(qPrintable(theme + "-wide")) << theme << QSize(920, 680);
      QTest::newRow(qPrintable(theme + "-compact")) << theme << QSize(480, 360);
    }
  }
  void actualWindow() {
    QFETCH(QString, theme);
    QFETCH(QSize, size);
    const QPalette oldPalette = QApplication::palette();
    QApplication::setPalette(fixturePalette(theme));
    QTemporaryDir files;
    const auto path = files.filePath("A little space.md");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray contents =
        "# A little space\n\nIdeas deserve a comfortable place to land.\n\n"
        "- One document. One window.\n- Arrange your thoughts with QindaQt "
        "containers.\n"
        "- Keep the details close and the page calm.\n\n"
        "## Tomorrow\n\nTake a walk. Notice something small. Write it down.\n";
    QCOMPARE(file.write(contents), contents.size());
    file.close();
    EditorApplication app([] { return std::make_unique<LocalDocumentStore>(); },
                          nullptr, nullptr, nullptr, {}, true);
    QVERIFY(app.start({path}));
    auto *window = app.windows().first();
    if (theme == QStringLiteral("high-contrast")) {
      static_cast<DocumentEditor *>(window->editor())->setHighContrast(true);
    }
    window->resize(size);
    QTRY_VERIFY(window->isVisible());
    window->editor()->setFocus();
    QTest::qWait(30);
    QCOMPARE(window->size(), size);
    QVERIFY(window->findChild<QToolBar *>()->isVisible());
    QVERIFY(window->editor()->width() > 300);
    QCOMPARE(window->editor()->toPlainText(), QString::fromUtf8(contents));
    const double minimum =
        theme == QStringLiteral("high-contrast") ? 7.0 : 4.5;
    const auto &palette = window->editor()->palette();
    QVERIFY(QindaQt::DesignTokens::DesignTokenDeriver::contrastRatio(
                palette.color(QPalette::Text), palette.color(QPalette::Base)) >=
            minimum);
    QVERIFY(QindaQt::DesignTokens::DesignTokenDeriver::contrastRatio(
                palette.color(QPalette::Text),
                palette.color(QPalette::AlternateBase)) >= minimum);
    for (auto block = window->editor()->document()->begin(); block.isValid();
         block = block.next()) {
      for (const auto &format : block.layout()->formats()) {
        const auto &brush = format.format.foreground();
        QVERIFY(brush.style() == Qt::NoBrush || brush.color().alpha() == 255);
        if (theme == QStringLiteral("high-contrast"))
          QCOMPARE(brush.style(), Qt::NoBrush);
        if (brush.style() != Qt::NoBrush) {
          using QindaQt::DesignTokens::DesignTokenDeriver;
          QVERIFY(DesignTokenDeriver::contrastRatio(
                      brush.color(), window->editor()->palette().color(
                                         QPalette::Base)) >= 4.5);
          QVERIFY(DesignTokenDeriver::contrastRatio(
                      brush.color(), window->editor()->palette().color(
                                         QPalette::AlternateBase)) >= 4.5);
        }
      }
    }
    const auto directory = QDir(QStringLiteral(QINDAQT_EDITOR_CAPTURE_DIR));
    QVERIFY(QDir().mkpath(directory.path()));
    QVERIFY(window->grab().save(directory.filePath(
        QString::fromLatin1(QTest::currentDataTag()) + ".png")));
    window->findChild<QAction *>(QStringLiteral("editFindAction"))->trigger();
    QTest::qWait(20);
    QCOMPARE(window->size(), size);
    QVERIFY(window->grab().save(directory.filePath(
        QString::fromLatin1(QTest::currentDataTag()) + "-search.png")));
    QApplication::setPalette(oldPalette);
  }
};
QTEST_MAIN(EditorVisualTest)
#include "tst_editor_visual.moc"
