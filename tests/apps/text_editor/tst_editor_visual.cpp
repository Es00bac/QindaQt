// SPDX-License-Identifier: GPL-3.0-or-later
#include "document/local_document_store.h"
#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/themes/theme_loader.h"
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
class EditorVisualTest final : public QObject {
  Q_OBJECT
private slots:
  void actualWindow_data() {
    QTest::addColumn<QString>("theme");
    QTest::addColumn<QSize>("size");
    for (const auto &theme :
         {QStringLiteral("qinda-dark"), QStringLiteral("qinda-light"),
          QStringLiteral("qinda-high-contrast")}) {
      QTest::newRow(qPrintable(theme + "-wide")) << theme << QSize(920, 680);
      QTest::newRow(qPrintable(theme + "-compact")) << theme << QSize(480, 360);
    }
  }
  void actualWindow() {
    QFETCH(QString, theme);
    QFETCH(QSize, size);
    auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/") + theme + ".json");
    QVERIFY(loaded.ok);
    auto appearance = EditorAppearanceAdapter::fromTheme(loaded.theme);
    QVERIFY(appearance.ok());
    QApplication::setPalette(appearance.appearance->palette);
    QApplication::setFont(appearance.appearance->interfaceFont);
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
                          *appearance.appearance, nullptr, nullptr, {}, true);
    QVERIFY(app.start({path}));
    auto *window = app.windows().first();
    window->resize(size);
    QTRY_VERIFY(window->isVisible());
    window->editor()->setFocus();
    QTest::qWait(30);
    QCOMPARE(window->size(), size);
    QVERIFY(window->findChild<QToolBar *>()->isVisible());
    QVERIFY(window->editor()->width() > 300);
    QCOMPARE(window->editor()->toPlainText(), QString::fromUtf8(contents));
    const double minimum =
        theme == QStringLiteral("qinda-high-contrast") ? 7.0 : 4.5;
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
        if (theme == QStringLiteral("qinda-high-contrast"))
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
  }
};
QTEST_MAIN(EditorVisualTest)
#include "tst_editor_visual.moc"
