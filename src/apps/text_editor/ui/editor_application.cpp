// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_application.h"
#include "editor_window.h"
#include "restore/text_editor_restore_policy.h"

#include <QEvent>
#include <QStatusBar>
#include <utility>

namespace QindaQt::Apps::TextEditor {
EditorApplication::EditorApplication(
    DocumentStoreFactory stores,
    TextEditorRestorePolicy *policy, RestoreStateStore *restoreStore,
    RecoveryJournalStore *journalStore,
    FileSelectionFactory choosers, bool showWindows,
    DocumentDialogFactory dialogs, QObject *parent)
    : QObject(parent), m_stores(std::move(stores)),
      m_policy(policy),
      m_restoreStore(restoreStore), m_journalStore(journalStore),
      m_choosers(std::move(choosers)),
      m_dialogs(std::move(dialogs)), m_showWindows(showWindows) {
  if (m_policy)
    connect(m_policy, &TextEditorRestorePolicy::policyChanged, this,
            &EditorApplication::policyChanged);
}

EditorApplication::~EditorApplication() {
  // QApplication remains alive while the composition owner tears down. Delete
  // views before their injected policy/store collaborators leave scope.
  const auto owned = windows();
  for (auto *window : owned)
    delete window;
}

QList<EditorWindow *> EditorApplication::windows() const {
  QList<EditorWindow *> result;
  for (const auto &window : m_windows)
    if (window)
      result.append(window);
  return result;
}

QStringList EditorApplication::openPaths() const {
  QStringList result;
  for (auto *window : windows()) {
    const auto &state = window->controller()->state();
    if (!state.isUntitled())
      result.append(state.path());
  }
  return result;
}

EditorWindow *EditorApplication::windowForPath(const QString &path) const {
  const auto canonical = DocumentController::normalizePath(path);
  if (canonical.isEmpty())
    return nullptr;
  for (auto *window : windows()) {
    if (window->controller()->state().path() == canonical)
      return window;
  }
  return nullptr;
}

EditorWindow *EditorApplication::createWindow() {
  if (windows().size() >= DocumentCollection::maximumDocuments) {
    report(tr("Document limit reached (%1 windows)")
               .arg(DocumentCollection::maximumDocuments));
    return nullptr;
  }
  auto *window = new EditorWindow(m_stores,
                                  m_choosers ? m_choosers() : nullptr, m_policy,
                                  this, m_dialogs ? m_dialogs() : nullptr,
                                  m_journalStore);
  window->setAttribute(Qt::WA_DeleteOnClose);
  m_windows.append(window);
  window->installEventFilter(this);
  connect(window, &EditorWindow::closeAccepted, this,
          [this, window] { windowClosed(window); });
  connect(window, &EditorWindow::documentInventoryChanged, this,
          &EditorApplication::persistRestoreState);
  emit windowCreated(window);
  return window;
}

void EditorApplication::present(EditorWindow *window) {
  if (!window)
    return;
  m_activeWindow = window;
  if (m_showWindows) {
    const bool firstShow = !window->isVisible();
    if (window->isMinimized())
      window->showNormal();
    else
      window->show();
    window->activateWindow();
    if (firstShow)
      emit windowShown(window);
  }
}

EditorWindow *EditorApplication::newWindow() {
  auto *window = createWindow();
  present(window);
  persistRestoreState();
  return window;
}

bool EditorApplication::start(const QStringList &paths, QString *diagnostic) {
  if (m_started)
    return openDocuments(paths, nullptr, diagnostic);
  m_started = true;
  m_explicitPaths = !paths.isEmpty();
  const bool opened = openDocuments(paths, nullptr, diagnostic);
  if (windows().isEmpty())
    (void)newWindow();
  policyChanged();
  sweepUntitledRecoveryJournals();
  return opened;
}

bool EditorApplication::openDocuments(const QStringList &paths,
                                      EditorWindow *preferred,
                                      QString *diagnostic) {
  if (paths.size() > DocumentCollection::maximumDocuments) {
    if (diagnostic)
      *diagnostic = tr("Too many document paths");
    return false;
  }
  // A batch must finish before any stateChanged emission can persist a partial
  // inventory. Restore uses the same admission path with the same guard.
  const bool previousAdmission = std::exchange(m_admitting, true);
  bool complete = true;
  for (const auto &path : paths) {
    if (auto *existing = windowForPath(path)) {
      present(existing);
      continue;
    }
    EditorWindow *target = preferred;
    if (!target || !windows().contains(target) ||
        !target->controller()->state().isUntitled() ||
        target->controller()->state().isDirty())
      target = nullptr;
    if (!target) {
      for (auto *candidate : windows()) {
        if (candidate->controller()->state().isUntitled() &&
            !candidate->controller()->state().isDirty()) {
          target = candidate;
          break;
        }
      }
    }
    const bool created = !target;
    if (created)
      target = createWindow();
    DocumentOperation result;
    if (target)
      result = target->controller()->openPath(path);
    else
      result = {.error = DocumentError::CapacityExceeded,
                .diagnostic = tr("Document window limit reached")};
    if (!result.ok()) {
      if (complete && diagnostic)
        *diagnostic = result.diagnostic.left(512);
      complete = false;
      if (created && target) {
        m_windows.removeAll(target);
        delete target;
      }
      continue;
    }
    present(target);
    target->offerRecoveryIfPresent();
  }
  m_admitting = previousAdmission;
  persistRestoreState();
  return complete;
}

bool EditorApplication::eventFilter(QObject *watched, QEvent *event) {
  if (event->type() == QEvent::WindowActivate) {
    if (auto *window = qobject_cast<EditorWindow *>(watched)) {
      m_activeWindow = window;
      persistRestoreState();
    }
  }
  return QObject::eventFilter(watched, event);
}

void EditorApplication::windowClosed(EditorWindow *window) {
  // The last close ends the application: retain its final path as the next
  // launch inventory. Closing one of several windows removes only that path.
  if (windows().size() == 1)
    persistRestoreState();
  m_windows.removeAll(window);
  if (m_activeWindow == window)
    m_activeWindow.clear();
  if (!windows().isEmpty())
    persistRestoreState();
}

void EditorApplication::report(const QString &message) {
  const auto bounded = message.left(512);
  emit diagnosticReported(bounded);
  if (m_activeWindow)
    m_activeWindow->announceStatus(bounded);
}

void EditorApplication::sweepUntitledRecoveryJournals() {
  if (!m_journalStore)
    return;
  const auto entries = m_journalStore->entries();
  for (const auto &entry : entries) {
    // Path journals are offered when their document opens; only orphan
    // untitled buffers need the startup sweep to be discoverable.
    if (entry.path.has_value())
      continue;
    if (windows().size() >= DocumentCollection::maximumDocuments) {
      report(tr("Recovery journals remain for more untitled documents than "
                "can be opened; they were left untouched"));
      break;
    }
    EditorWindow *window = createWindow();
    if (!window)
      break;
    if (window->offerUntitledRecovery(entry)) {
      present(window);
    } else {
      m_windows.removeAll(window);
      delete window;
    }
  }
}
} // namespace QindaQt::Apps::TextEditor
