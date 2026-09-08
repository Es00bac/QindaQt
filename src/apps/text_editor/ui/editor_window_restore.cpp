// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_window.h"

#include "editor_application.h"
#include "restore/text_editor_restore_policy.h"

#include "qindaqt/app_shell/app_shell_types.h"

#include <QAction>

namespace QindaQt::Apps::TextEditor {

void EditorWindow::connectRestorePolicy() {
  using QindaQt::AppShell::IntegrationState;
  if (!m_restorePolicy) {
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

void EditorWindow::persistRestoreState() {
  if (m_application) m_application->persistRestoreState();
}

} // namespace QindaQt::Apps::TextEditor
