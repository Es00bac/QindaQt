// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_editor.h"
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/SyntaxHighlighter>
#include <KSyntaxHighlighting/Theme>
#include <QKeyEvent>
#include <QPainter>
#include <QTextBlock>
#include <algorithm>

namespace QindaQt::Apps::TextEditor {
namespace {
class LineNumberGutter final : public QWidget {
public:
  explicit LineNumberGutter(DocumentEditor *editor)
      : QWidget(editor), m_editor(editor) {
    setObjectName(QStringLiteral("lineNumberGutter"));
  }

protected:
  void paintEvent(QPaintEvent *event) override { m_editor->paintGutter(event); }

private:
  DocumentEditor *m_editor;
};
} // namespace
struct DocumentEditor::Syntax {
  KSyntaxHighlighting::Repository repository;
  KSyntaxHighlighting::SyntaxHighlighter highlighter;
  explicit Syntax(QTextDocument *document) : highlighter(document) {}
};
DocumentEditor::DocumentEditor(QWidget *parent)
    : QPlainTextEdit(parent), m_syntax(std::make_unique<Syntax>(document())),
      m_gutter(new LineNumberGutter(this)), m_baseFont(font()) {
  connect(this, &QPlainTextEdit::blockCountChanged, this,
          &DocumentEditor::updateGutter);
  connect(this, &QPlainTextEdit::updateRequest, this,
          [this](const QRect &rect, int dy) {
            if (dy)
              m_gutter->scroll(0, dy);
            else
              m_gutter->update(0, rect.y(), gutterWidth(), rect.height());
          });
  connect(this, &QPlainTextEdit::cursorPositionChanged, this,
          &DocumentEditor::highlightCurrentLine);
  setBaseFont(font());
  highlightCurrentLine();
}
DocumentEditor::~DocumentEditor() = default;
int DocumentEditor::gutterWidth() const {
  return 16 + fontMetrics().horizontalAdvance(QLatin1Char('9')) *
                  static_cast<int>(QString::number(blockCount()).size());
}
void DocumentEditor::updateGutter() {
  setViewportMargins(gutterWidth(), 0, 0, 0);
  const QRect area = contentsRect();
  m_gutter->setGeometry(area.left(), area.top(), gutterWidth(), area.height());
  m_gutter->update();
}
void DocumentEditor::resizeEvent(QResizeEvent *event) {
  QPlainTextEdit::resizeEvent(event);
  updateGutter();
}
void DocumentEditor::paintGutter(QPaintEvent *event) {
  QPainter painter(m_gutter);
  painter.fillRect(event->rect(), palette().alternateBase());
  QTextBlock block = firstVisibleBlock();
  int number = block.blockNumber();
  qreal top = blockBoundingGeometry(block).translated(contentOffset()).top();
  while (block.isValid() && top <= event->rect().bottom()) {
    const qreal height = blockBoundingRect(block).height();
    if (block.isVisible() && top + height >= event->rect().top()) {
      painter.setPen(number == textCursor().blockNumber()
                         ? palette().text().color()
                         : palette().placeholderText().color());
      painter.drawText(0, qRound(top), gutterWidth() - 8,
                       fontMetrics().height(), Qt::AlignRight,
                       QString::number(number + 1));
    }
    top += height;
    block = block.next();
    ++number;
  }
}
void DocumentEditor::highlightCurrentLine() {
  QTextEdit::ExtraSelection line;
  line.format.setBackground(palette().alternateBase());
  line.format.setProperty(QTextFormat::FullWidthSelection, true);
  line.cursor = textCursor();
  line.cursor.clearSelection();
  setExtraSelections({line});
  m_gutter->update();
}
void DocumentEditor::setDocumentPath(const QString &path) {
  if (m_path == path)
    return;
  m_path = path;
  m_syntax->highlighter.setDefinition(
      m_syntax->repository.definitionForFileName(path));
  m_syntax->highlighter.setTheme(m_syntax->repository.defaultTheme(
      palette().base().color().lightness() < 128
          ? KSyntaxHighlighting::Repository::DarkTheme
          : KSyntaxHighlighting::Repository::LightTheme));
}
QString DocumentEditor::syntaxName() const {
  const auto definition = m_syntax->highlighter.definition();
  return definition.isValid() ? definition.translatedName() : tr("Plain text");
}
void DocumentEditor::setBaseFont(const QFont &value) {
  m_baseFont = value;
  QFont scaled = value;
  scaled.setPointSizeF(std::clamp(value.pointSizeF() + m_zoom, 6.0, 48.0));
  setFont(scaled);
  setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);
  updateGutter();
}
void DocumentEditor::zoomText(int steps) {
  m_zoom = std::clamp(m_zoom + steps, -6, 24);
  setBaseFont(m_baseFont);
}
void DocumentEditor::resetZoom() {
  m_zoom = 0;
  setBaseFont(m_baseFont);
}
void DocumentEditor::changeEvent(QEvent *event) {
  QPlainTextEdit::changeEvent(event);
  if (event->type() == QEvent::PaletteChange && m_syntax) {
    m_syntax->highlighter.setTheme(m_syntax->repository.defaultTheme(
        palette().base().color().lightness() < 128
            ? KSyntaxHighlighting::Repository::DarkTheme
            : KSyntaxHighlighting::Repository::LightTheme));
    highlightCurrentLine();
  }
}
void DocumentEditor::goToLine(int line) {
  QTextCursor cursor(
      document()->findBlockByNumber(std::clamp(line, 1, blockCount()) - 1));
  setTextCursor(cursor);
  centerCursor();
  setFocus();
}
void DocumentEditor::indentLines(bool remove) {
  QTextCursor saved = textCursor();
  QTextBlock first = document()->findBlock(saved.selectionStart());
  QTextBlock last = document()->findBlock(saved.selectionEnd());
  if (saved.hasSelection() && last.position() == saved.selectionEnd())
    last = last.previous();
  QTextCursor edit(document());
  edit.beginEditBlock();
  for (QTextBlock block = last; block.isValid(); block = block.previous()) {
    edit.setPosition(block.position());
    if (!remove)
      edit.insertText(QStringLiteral("    "));
    else {
      const QString text = block.text();
      int count = 0;
      if (text.startsWith(QLatin1Char('\t')))
        count = 1;
      else
        while (count < std::min(4, static_cast<int>(text.size())) &&
               text.at(count) == QLatin1Char(' '))
          ++count;
      edit.setPosition(block.position() + count, QTextCursor::KeepAnchor);
      edit.removeSelectedText();
    }
    if (block == first)
      break;
  }
  edit.endEditBlock();
  setTextCursor(saved);
}
void DocumentEditor::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Backtab) {
    indentLines(true);
    return;
  }
  if (event->key() == Qt::Key_Tab && event->modifiers() == Qt::NoModifier) {
    if (textCursor().hasSelection())
      indentLines(false);
    else {
      auto cursor = textCursor();
      cursor.insertText(
          QString(4 - cursor.positionInBlock() % 4, QLatin1Char(' ')));
    }
    return;
  }
  if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
      event->modifiers() == Qt::NoModifier && !textCursor().hasSelection()) {
    auto cursor = textCursor();
    const QString prefix = cursor.block().text().left(cursor.positionInBlock());
    int size = 0;
    while (size < prefix.size() && (prefix.at(size) == QLatin1Char(' ') ||
                                    prefix.at(size) == QLatin1Char('\t')))
      ++size;
    cursor.beginEditBlock();
    cursor.insertText(QLatin1Char('\n') + prefix.left(size));
    cursor.endEditBlock();
    return;
  }
  QPlainTextEdit::keyPressEvent(event);
}
} // namespace QindaQt::Apps::TextEditor
