// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_document_view.h"
#include "document_editor.h"

#include <QAccessible>
#include <QAccessibleAnnouncementEvent>
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyle>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>

#include <utility>

namespace QindaQt::Apps::TextEditor {

EditorDocumentView::EditorDocumentView(DocumentController *controller,
                                       QWidget *parent)
    : QWidget(parent), m_controller(controller) {
  Q_ASSERT(m_controller);
  setObjectName(QStringLiteral("editorDocumentView"));
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_externalBanner = new QWidget(this);
  m_externalBanner->setObjectName(QStringLiteral("externalChangeBanner"));
  m_externalBanner->setAccessibleName(tr("External file change warning"));
  auto *bannerLayout = new QHBoxLayout(m_externalBanner);
  bannerLayout->setContentsMargins(12, 8, 12, 8);
  m_externalIcon = new QLabel(m_externalBanner);
  m_externalIcon->setObjectName(QStringLiteral("externalChangeIcon"));
  m_externalLabel = new QLabel(m_externalBanner);
  m_externalLabel->setObjectName(QStringLiteral("externalChangeMessage"));
  m_externalLabel->setWordWrap(true);
  m_externalLabel->setAccessibleName(tr("External file status"));
  m_reloadButton = new QPushButton(tr("&Reload"), m_externalBanner);
  m_reloadButton->setObjectName(QStringLiteral("reloadExternalAction"));
  m_reloadButton->setAccessibleName(tr("Reload file from disk"));
  m_saveAsButton = new QPushButton(tr("Save &As…"), m_externalBanner);
  m_saveAsButton->setObjectName(QStringLiteral("saveAsExternalAction"));
  m_saveAsButton->setAccessibleName(
      tr("Save this document under a different name"));
  bannerLayout->addWidget(m_externalIcon);
  bannerLayout->addWidget(m_externalLabel, 1);
  bannerLayout->addWidget(m_reloadButton);
  bannerLayout->addWidget(m_saveAsButton);
  m_externalBanner->hide();

  m_editor = new DocumentEditor(this);
  m_editor->setObjectName(QStringLiteral("documentEditor"));
  m_editor->setAccessibleName(tr("Document text"));
  m_editor->setAccessibleDescription(
      tr("Edit the current local UTF-8 plain-text document"));
  m_editor->setTabChangesFocus(false);
  m_editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
  layout->addWidget(m_externalBanner);
  layout->addWidget(m_editor, 1);
  setTabOrder(m_editor, m_reloadButton);
  setTabOrder(m_reloadButton, m_saveAsButton);
  setTabOrder(m_saveAsButton, m_editor);

  m_replacingContents = true;
  m_editor->setPlainText(m_controller->state().text());
  m_editor->document()->setModified(m_controller->state().isDirty());
  m_replacingContents = false;
  static_cast<DocumentEditor *>(m_editor)->setDocumentPath(m_controller->state().path());
  connectState();
  updateExternalBanner(m_controller->state().externalState());
}

void EditorDocumentView::connectState() {
  connect(m_editor, &QPlainTextEdit::cursorPositionChanged, this,
          &EditorDocumentView::presentationChanged);
  connect(m_editor->document(), &QTextDocument::contentsChange, this,
          [this](int position, int charsRemoved, int charsAdded) {
            if (m_replacingContents) {
              return;
            }
            // QTextDocument counts its terminal paragraph marker in a bulk
            // replacement's charsAdded value. Plain text does not, so slicing
            // the authoritative plain-text projection both clamps that marker
            // and preserves newline offsets without an out-of-range cursor.
            const QString insertedText =
                m_editor->toPlainText().mid(position, charsAdded);
            m_controller->applyTextEdit(position, charsRemoved, insertedText);
          });
  connect(m_controller, &DocumentController::contentsReplacementRequested, this,
          [this](const QString &text) {
            m_replacingContents = true;
            m_editor->setPlainText(text);
            m_editor->document()->setModified(false);
            m_replacingContents = false;
            m_editor->moveCursor(QTextCursor::Start);
            m_editor->setFocus(Qt::OtherFocusReason);
          });
  connect(m_controller, &DocumentController::stateChanged, this, [this] {
    m_editor->document()->setModified(m_controller->state().isDirty());
    static_cast<DocumentEditor *>(m_editor)->setDocumentPath(m_controller->state().path());
    updateExternalBanner(m_controller->state().externalState());
    emit presentationChanged();
  });
  connect(m_controller, &DocumentController::externalStateChanged, this,
          &EditorDocumentView::announceExternalState);
  connect(m_reloadButton, &QPushButton::clicked, this,
          &EditorDocumentView::reloadRequested);
  connect(m_saveAsButton, &QPushButton::clicked, this,
          &EditorDocumentView::saveAsRequested);
}

void EditorDocumentView::updateExternalBanner(const ExternalState state) {
  if (m_renderedExternalState == state) {
    return;
  }
  m_renderedExternalState = state;
  if (state == ExternalState::InSync) {
    QWidget *focused = QApplication::focusWidget();
    const bool recoveryHadFocus =
        focused && (focused == m_externalBanner ||
                    m_externalBanner->isAncestorOf(focused));
    m_externalBanner->hide();
    if (recoveryHadFocus) {
      m_editor->setFocus(Qt::OtherFocusReason);
    }
    return;
  }

  // AGENT-CONTRACT: The banner's severity is carried by the style's standard
  // icon plus explicit text and accessibility announcements — never by a
  // token-colored QSS surface (ADR-0116). Meaning never depends on color.
  refreshExternalIcon();
  QString text;
  if (state == ExternalState::Changed) {
    text = tr("Warning: This file changed outside QindaQt Text Editor. Reload "
              "it or save your text under a different name.");
  } else if (state == ExternalState::Missing) {
    text = tr("Error: This file was removed outside QindaQt Text Editor. Save "
              "your text under a different name.");
  } else {
    text = tr("Error: This file can no longer be checked. Saving over it is "
              "blocked; use Save As.");
  }
  m_externalLabel->setText(text);
  m_externalLabel->setAccessibleDescription(text);
  m_reloadButton->setEnabled(state == ExternalState::Changed);
  m_externalBanner->show();
}

void EditorDocumentView::refreshExternalIcon() {
  const bool warning = m_renderedExternalState == ExternalState::Changed;
  const auto icon = style()->standardIcon(
      warning ? QStyle::SP_MessageBoxWarning : QStyle::SP_MessageBoxCritical);
  const int size = style()->pixelMetric(QStyle::PM_SmallIconSize);
  m_externalIcon->setPixmap(icon.pixmap(size, size));
  m_externalIcon->setAccessibleName(warning ? tr("Warning") : tr("Error"));
}

void EditorDocumentView::changeEvent(QEvent *event) {
  QWidget::changeEvent(event);
  if (m_externalIcon && (event->type() == QEvent::StyleChange ||
                         event->type() == QEvent::PaletteChange ||
                         event->type() == QEvent::ThemeChange)) {
    refreshExternalIcon();
  }
}

void EditorDocumentView::announceExternalState(const ExternalState state) {
  if (state == ExternalState::InSync) {
    return;
  }
  QAccessibleAnnouncementEvent event(m_externalLabel, m_externalLabel->text());
  event.setPoliteness(QAccessible::AnnouncementPoliteness::Assertive);
  QAccessible::updateAccessibility(&event);
}

QString EditorDocumentView::statusText() const {
  const DocumentState &state = m_controller->state();
  if (state.externalState() == ExternalState::Changed) {
    return tr("File changed outside the editor");
  }
  if (state.externalState() == ExternalState::Missing) {
    return tr("File was removed outside the editor");
  }
  if (state.externalState() == ExternalState::Unreadable) {
    return tr("File can no longer be checked");
  }
  const auto cursor = m_editor->textCursor();
  return tr("Ln %1, Col %2  ·  %3  ·  UTF-8%4")
      .arg(cursor.blockNumber() + 1).arg(cursor.positionInBlock() + 1)
      .arg(static_cast<DocumentEditor *>(m_editor)->syntaxName())
      .arg(state.isDirty() ? tr("  ·  Modified") : QString{});
}

} // namespace QindaQt::Apps::TextEditor
