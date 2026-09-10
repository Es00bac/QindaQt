// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_editor.h"
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Format>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/SyntaxHighlighter>
#include <KSyntaxHighlighting/Theme>
#include <QAccessibilityHints>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPainter>
#include <QStyleHints>
#include <QTextBlock>
#include <algorithm>
#include <cmath>

namespace QindaQt::Apps::TextEditor {
namespace {

// AGENT-CONTRACT: Content readability math mirrors the WCAG relative-luminance
// contrast ratio that QST's DesignTokenDeriver::contrastRatio implements. The
// editor must not link DesignTokens (ADR-0116), so the pure ratio lives here;
// keep the formulas behaviorally identical with src/design_tokens/src.
QColor compositeOver(const QColor &foreground, const QColor &background) {
  const double foregroundAlpha = foreground.alphaF();
  const double backgroundAlpha = background.alphaF();
  const double outputAlpha =
      foregroundAlpha + backgroundAlpha * (1.0 - foregroundAlpha);
  if (outputAlpha <= 0.0)
    return QColor::fromRgbF(0.0, 0.0, 0.0, 0.0);
  const double red = (foreground.redF() * foregroundAlpha +
                      background.redF() * backgroundAlpha * (1.0 - foregroundAlpha)) /
                     outputAlpha;
  const double green = (foreground.greenF() * foregroundAlpha +
                        background.greenF() * backgroundAlpha * (1.0 - foregroundAlpha)) /
                       outputAlpha;
  const double blue = (foreground.blueF() * foregroundAlpha +
                       background.blueF() * backgroundAlpha * (1.0 - foregroundAlpha)) /
                      outputAlpha;
  return QColor::fromRgbF(
      static_cast<float>(std::clamp(red, 0.0, 1.0)),
      static_cast<float>(std::clamp(green, 0.0, 1.0)),
      static_cast<float>(std::clamp(blue, 0.0, 1.0)), 1.0F);
}

double relativeLuminance(const QColor &color) {
  const auto channel = [](double value) {
    return value <= 0.03928 ? value / 12.92
                            : std::pow((value + 0.055) / 1.055, 2.4);
  };
  return 0.2126 * channel(color.redF()) + 0.7152 * channel(color.greenF()) +
         0.0722 * channel(color.blueF());
}

double contrastRatio(const QColor &foreground, const QColor &background) {
  const QColor opaqueBackground = background.alphaF() < 1.0
      ? compositeOver(background, QColor(Qt::white))
      : background;
  const QColor opaqueForeground = foreground.alphaF() < 1.0
      ? compositeOver(foreground, opaqueBackground)
      : foreground;
  const double fg = relativeLuminance(opaqueForeground);
  const double bg = relativeLuminance(opaqueBackground);
  return (std::max(fg, bg) + 0.05) / (std::min(fg, bg) + 0.05);
}

QFont platformDocumentFont() {
  return QFontDatabase::systemFont(QFontDatabase::FixedFont);
}

class ReadableSyntaxHighlighter final : public KSyntaxHighlighting::SyntaxHighlighter {
public:
  explicit ReadableSyntaxHighlighter(QPlainTextEdit *editor)
      : SyntaxHighlighter(editor->document()), m_editor(editor) {}
  void setHighContrast(bool enabled) {
    m_highContrast = enabled;
    rehighlight();
  }
protected:
  void applyFormat(int offset, int length, const KSyntaxHighlighting::Format &syntax) override {
    SyntaxHighlighter::applyFormat(offset, length, syntax);
    auto value = QSyntaxHighlighter::format(offset);
    // AGENT-GUARD: Upstream syntax colors are not fitted to the platform theme.
    // Transparent headings or low-contrast tokens must retain readable semantic
    // ink, including against the current-line surface, while keeping emphasis.
    // Truth is the widget's live palette(), never a QST token (ADR-0116).
    const auto &palette = m_editor->palette();
    const QColor foreground = value.foreground().style() == Qt::NoBrush
        ? palette.color(QPalette::Text) : value.foreground().color();
    const QColor background = value.background().style() == Qt::NoBrush
        ? palette.color(QPalette::Base) : value.background().color();
    const bool unreadable = foreground.alpha() < 255 ||
        contrastRatio(foreground, background) < 4.5 ||
        contrastRatio(foreground, palette.color(QPalette::AlternateBase)) < 4.5;
    if (m_highContrast || unreadable) {
      value.clearForeground();
      value.clearBackground();
      setFormat(offset, length, value);
    }
  }
private:
  QPlainTextEdit *m_editor;
  bool m_highContrast = false;
};
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
  ReadableSyntaxHighlighter highlighter;
  explicit Syntax(QPlainTextEdit *editor) : highlighter(editor) {}
};
DocumentEditor::DocumentEditor(QWidget *parent)
    : QPlainTextEdit(parent), m_syntax(std::make_unique<Syntax>(this)),
      m_gutter(new LineNumberGutter(this)),
      m_baseFont(platformDocumentFont()) {
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
  // High-contrast truth is the platform theme's contrast preference
  // (ADR-0115); it is a content-readability input, not chrome.
  if (auto *hints = QGuiApplication::styleHints()->accessibility()) {
    setHighContrast(hints->contrastPreference() ==
                    Qt::ContrastPreference::HighContrast);
    connect(hints, &QAccessibilityHints::contrastPreferenceChanged, this,
            [this](Qt::ContrastPreference preference) {
              setHighContrast(preference == Qt::ContrastPreference::HighContrast);
            });
  }
  setFrameShape(QFrame::NoFrame);
  document()->setDocumentMargin(12);
  setBaseFont(m_baseFont);
  highlightCurrentLine();
}
DocumentEditor::~DocumentEditor() = default;
int DocumentEditor::gutterWidth() const {
  return 24 + fontMetrics().horizontalAdvance(QLatin1Char('9')) *
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
void DocumentEditor::setHighContrast(bool enabled) {
  m_syntax->highlighter.setHighContrast(enabled);
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
  if (!m_syntax)
    return;
  if (event->type() == QEvent::PaletteChange ||
      event->type() == QEvent::ThemeChange) {
    m_syntax->highlighter.setTheme(m_syntax->repository.defaultTheme(
        palette().base().color().lightness() < 128
            ? KSyntaxHighlighting::Repository::DarkTheme
            : KSyntaxHighlighting::Repository::LightTheme));
    m_syntax->highlighter.rehighlight();
    highlightCurrentLine();
  }
  // The document font pins WA_SetFont for zoom, so it cannot follow the
  // platform theme implicitly; re-derive the fixed font on application font
  // changes while preserving this window's zoom offset.
  if (event->type() == QEvent::ApplicationFontChange) {
    const QFont platformFont = platformDocumentFont();
    if (platformFont != m_baseFont)
      setBaseFont(platformFont);
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
