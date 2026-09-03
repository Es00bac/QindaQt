// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_window.h"

#include "restore/restore_state_store.h"
#include "restore/text_editor_restore_policy.h"

#include "qindaqt/app_shell/app_shell_types.h"

#include <QAction>

namespace QindaQt::Apps::TextEditor {

void EditorWindow::connectRestorePolicy() {
  using QindaQt::AppShell::IntegrationState;
  if (!m_restorePolicy || !m_restoreStore) {
    m_actions.restoreDocuments->setEnabled(false);
    m_appShellBridge.coordinator().setSettingsState(
        IntegrationState::NotRequired);
    return;
  }
  const auto publish = [this] {
    const bool ready = m_restorePolicy->baselineReceived();
    m_actions.restoreDocuments->setEnabled(ready &&
                                           !m_restorePolicy->writePending());
    m_actions.restoreDocuments->setChecked(m_restorePolicy->enabled());
    m_appShellBridge.coordinator().setSettingsState(
        ready ? IntegrationState::Ready : IntegrationState::Unavailable,
        ready ? QString() : tr("Document restore policy is unavailable"));
    if (m_restorePolicy->enabled()) {
      restoreIfEnabled();
      persistRestoreState();
    } else {
      const RestoreWriteResult cleared = m_restoreStore->clear();
      if (!cleared.ok()) {
        announceStatus(cleared.diagnostic);
      }
    }
  };
  connect(m_restorePolicy, &TextEditorRestorePolicy::policyChanged, this,
          publish);
  connect(m_restorePolicy, &TextEditorRestorePolicy::applyFinished, this,
          [this](RestorePolicyResult result, const QString &message) {
            if (result == RestorePolicyResult::Applied) {
              announceStatus(message);
            } else if (result == RestorePolicyResult::Conflict) {
              announceStatus(tr("Restore policy conflict; review and retry"));
            } else if (result == RestorePolicyResult::Uncertain) {
              announceStatus(
                  tr("Restore policy outcome uncertain; not replayed"));
            } else {
              announceStatus(message.isEmpty()
                                 ? tr("Restore policy could not be saved")
                                 : message);
            }
          });
  publish();
}

void EditorWindow::restoreIfEnabled() {
  if (m_restoreAttempted || m_explicitStartupPaths || !m_restorePolicy ||
      !m_restoreStore || !m_restorePolicy->baselineReceived() ||
      !m_restorePolicy->enabled()) {
    return;
  }
  m_restoreAttempted = true;
  const RestoreLoadResult loaded = m_restoreStore->load();
  if (!loaded.ok()) {
    if (loaded.error != RestoreStateError::Absent) {
      announceStatus(
          tr("Saved document list was rejected: %1").arg(loaded.diagnostic));
    }
    return;
  }

  int skipped = 0;
  int selected = -1;
  for (int sourceIndex = 0; sourceIndex < loaded.state->paths.size();
       ++sourceIndex) {
    QString diagnostic;
    const QString &path = loaded.state->paths.at(sourceIndex);
    if (!addPath(path, &diagnostic)) {
      ++skipped;
      continue;
    }
    if (sourceIndex == loaded.state->activeIndex) {
      selected = m_documents->indexOfPath(path);
    }
  }
  if (selected >= 0) {
    selectTab(selected);
  }
  if (skipped > 0) {
    announceStatus(tr("Skipped %1 unavailable saved document(s)").arg(skipped));
  }
  persistRestoreState();
}

void EditorWindow::persistRestoreState() {
  if (!m_restorePolicy || !m_restoreStore ||
      !m_restorePolicy->baselineReceived() || !m_restorePolicy->enabled()) {
    return;
  }
  // AGENT-GUARD: Do not let the constructor's placeholder document erase a
  // persisted inventory before an already-enabled policy has restored it.
  if (!m_restoreAttempted && !m_explicitStartupPaths) {
    return;
  }
  RestoreState state;
  const int activeDocument = m_tabs->currentIndex();
  for (int index = 0; index < m_documents->count(); ++index) {
    DocumentController *document = m_documents->at(index);
    if (!document || document->state().isUntitled()) {
      continue;
    }
    if (index == activeDocument) {
      state.activeIndex = static_cast<int>(state.paths.size());
    }
    state.paths.append(document->state().path());
  }
  if (!state.paths.isEmpty() && state.activeIndex < 0) {
    state.activeIndex = 0;
  }
  const RestoreWriteResult result = m_restoreStore->store(state);
  if (!result.ok()) {
    announceStatus(result.diagnostic);
  }
}

} // namespace QindaQt::Apps::TextEditor
