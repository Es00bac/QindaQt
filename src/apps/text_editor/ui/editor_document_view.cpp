// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_document_view.h"

#include <QAccessible>
#include <QAccessibleAnnouncementEvent>
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>

#include <utility>

namespace QindaQt::Apps::TextEditor {
namespace {

QString colorCss(const QColor &color) { return color.name(QColor::HexArgb); }

} // namespace

EditorDocumentView::EditorDocumentView(DocumentController *controller,
                                       const EditorAppearance &appearance,
                                       QWidget *parent)
    : QWidget(parent), m_controller(controller), m_appearance(appearance) {
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
  bannerLayout->addWidget(m_externalLabel, 1);
  bannerLayout->addWidget(m_reloadButton);
  bannerLayout->addWidget(m_saveAsButton);
  m_externalBanner->hide();

  m_editor = new QPlainTextEdit(this);
  m_editor->setObjectName(QStringLiteral("documentEditor"));
  m_editor->setAccessibleName(tr("Document text"));
  m_editor->setAccessibleDescription(
      tr("Edit the current local UTF-8 plain-text document"));
  m_editor->setTabChangesFocus(false);
  m_editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
  m_editor->setFont(m_appearance.editorFont);
  layout->addWidget(m_externalBanner);
  layout->addWidget(m_editor, 1);
  setTabOrder(m_editor, m_reloadButton);
  setTabOrder(m_reloadButton, m_saveAsButton);
  setTabOrder(m_saveAsButton, m_editor);

  m_replacingContents = true;
  m_editor->setPlainText(m_controller->state().text());
  m_editor->document()->setModified(m_controller->state().isDirty());
  m_replacingContents = false;
  connectState();
  updateExternalBanner(m_controller->state().externalState());
}

void EditorDocumentView::connectState() {
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

  const bool warning = state == ExternalState::Changed;
  const QColor background =
      warning ? m_appearance.warningBackground : m_appearance.dangerBackground;
  const QColor foreground =
      warning ? m_appearance.warningForeground : m_appearance.dangerForeground;
  m_externalBanner->setStyleSheet(
      QStringLiteral("#externalChangeBanner { background: %1; border-bottom: "
                     "1px solid %2; } #externalChangeMessage { color: %3; } "
                     "#externalChangeBanner QPushButton:focus { border: 2px "
                     "solid %4; border-radius: %5px; }")
          .arg(colorCss(background), colorCss(foreground), colorCss(foreground),
               colorCss(m_appearance.focusRing),
               QString::number(m_appearance.mediumRadius)));
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
  if (state.isDirty()) {
    return tr("Modified");
  }
  return state.isUntitled() ? tr("Ready") : tr("Saved");
}

} // namespace QindaQt::Apps::TextEditor
