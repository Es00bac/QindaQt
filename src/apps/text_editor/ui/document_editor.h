// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QPlainTextEdit>
#include <memory>

namespace QindaQt::Apps::TextEditor {
// Per-document text presentation; persistence and document identity stay in
// DocumentController. All edits use QTextCursor and the existing undo pipeline.
// Appearance follows the widget's own palette()/font() (Qt platform theme and
// Fusion per ADR-0116); this class never reads QST tokens or theme files.
class DocumentEditor final : public QPlainTextEdit {
  Q_OBJECT
public:
  explicit DocumentEditor(QWidget *parent = nullptr);
  ~DocumentEditor() override;
  void setDocumentPath(const QString &path);
  void setBaseFont(const QFont &font);
  [[nodiscard]] QFont baseFont() const { return m_baseFont; }
  // Test and future-composition override; production truth is the platform
  // theme's Qt::ContrastPreference, tracked internally.
  void setHighContrast(bool enabled);
  void zoomText(int steps);
  void resetZoom();
  void indentLines(bool remove);
  void goToLine(int line);
  [[nodiscard]] QString syntaxName() const;
  [[nodiscard]] int gutterWidth() const;
  void paintGutter(QPaintEvent *event);

protected:
  void resizeEvent(QResizeEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void changeEvent(QEvent *event) override;

private:
  void updateGutter();
  void highlightCurrentLine();
  struct Syntax;
  std::unique_ptr<Syntax> m_syntax;
  QWidget *m_gutter;
  QFont m_baseFont;
  int m_zoom = 0;
  QString m_path;
};
} // namespace QindaQt::Apps::TextEditor
