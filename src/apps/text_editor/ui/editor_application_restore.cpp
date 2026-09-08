// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_application.h"
#include "editor_window.h"
#include "restore/text_editor_restore_policy.h"

namespace QindaQt::Apps::TextEditor {
void EditorApplication::policyChanged() {
  if (!m_started || !m_policy || !m_restoreStore)
    return;
  // Initial Settings1 acquisition is asynchronous. Its temporary absence must
  // not erase a saved inventory before the first confirmed enabled snapshot.
  // Once truth has been seen, owner loss still clears state fail-closed.
  if (m_policy->baselineReceived())
    m_policyBaselineSeen = true;
  else if (!m_policyBaselineSeen)
    return;
  if (m_policy->enabled()) {
    restoreIfEnabled();
    persistRestoreState();
  } else {
    m_lastStored.reset();
    const auto result = m_restoreStore->clear();
    if (!result.ok())
      report(result.diagnostic);
  }
}

void EditorApplication::restoreIfEnabled() {
  if (m_restoreAttempted || m_explicitPaths || !m_policy || !m_restoreStore ||
      !m_policy->baselineReceived() || !m_policy->enabled())
    return;
  m_restoreAttempted = true;
  const auto loaded = m_restoreStore->load();
  if (!loaded.ok()) {
    if (loaded.error != RestoreStateError::Absent)
      report(tr("Saved document list was rejected: %1").arg(loaded.diagnostic));
    return;
  }
  m_admitting = true;
  int skipped = 0;
  for (const auto &path : loaded.state->paths) {
    if (!openDocuments({path}))
      ++skipped;
  }
  if (loaded.state->activeIndex >= 0) {
    if (auto *preferred =
            windowForPath(loaded.state->paths.at(loaded.state->activeIndex)))
      present(preferred);
  }
  m_admitting = false;
  persistRestoreState();
  if (skipped)
    report(tr("Skipped %1 unavailable saved document(s)").arg(skipped));
}

void EditorApplication::persistRestoreState() {
  if (!m_started || m_admitting || !m_policy || !m_restoreStore ||
      !m_policy->baselineReceived() || !m_policy->enabled() ||
      (!m_restoreAttempted && !m_explicitPaths))
    return;
  RestoreState state;
  for (auto *window : windows()) {
    const auto &document = window->controller()->state();
    if (document.isUntitled())
      continue;
    if (window == m_activeWindow)
      state.activeIndex = int(state.paths.size());
    state.paths.append(document.path());
  }
  if (!state.paths.isEmpty() && state.activeIndex < 0)
    state.activeIndex = 0;
  if (m_lastStored && *m_lastStored == state)
    return;
  const auto result = m_restoreStore->store(state);
  if (result.ok())
    m_lastStored = state;
  else
    report(result.diagnostic);
}
} // namespace QindaQt::Apps::TextEditor
