// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "document/document_collection.h"
#include "document_dialogs.h"
#include "restore/restore_state_store.h"

#include <QObject>
#include <QPointer>
#include <functional>

namespace QindaQt::Apps::TextEditor {
class EditorWindow;
class FileSelectionAdapter;
class RecoveryJournalStore;
class TextEditorRestorePolicy;

// GUI-thread owner of ordinary document windows and the process-local path
// inventory. Windows own their controller/view and never own one another.
// Policy/store are borrowed; they must outlive this owner. The optional chooser
// factory transfers one adapter into each window. No global window registry or
// cross-process document lock is implied by canonical-path uniqueness here.
class EditorApplication final : public QObject {
  Q_OBJECT
public:
  using FileSelectionFactory =
      std::function<std::unique_ptr<FileSelectionAdapter>()>;
  using DocumentDialogFactory =
      std::function<std::unique_ptr<DocumentDialogs>()>;
  explicit EditorApplication(DocumentStoreFactory stores,
                             TextEditorRestorePolicy *policy = nullptr,
                             RestoreStateStore *restoreStore = nullptr,
                             RecoveryJournalStore *journalStore = nullptr,
                             FileSelectionFactory choosers = {},
                             bool showWindows = true,
                             DocumentDialogFactory dialogs = {},
                             QObject *parent = nullptr);
  ~EditorApplication() override;

  // start fixes CLI precedence before an already-confirmed restore policy can
  // load state. A failed path leaves all successfully admitted windows intact.
  [[nodiscard]] bool start(const QStringList &paths = {},
                           QString *diagnostic = nullptr);
  [[nodiscard]] EditorWindow *newWindow();
  [[nodiscard]] bool openDocuments(const QStringList &paths,
                                   EditorWindow *preferred = nullptr,
                                   QString *diagnostic = nullptr);
  [[nodiscard]] QList<EditorWindow *> windows() const;
  [[nodiscard]] EditorWindow *windowForPath(const QString &path) const;
  [[nodiscard]] QStringList openPaths() const;
  void persistRestoreState();

signals:
  void windowCreated(EditorWindow *window);
  void windowShown(EditorWindow *window);
  void diagnosticReported(const QString &message);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  EditorWindow *createWindow();
  void present(EditorWindow *window);
  void policyChanged();
  void restoreIfEnabled();
  void sweepUntitledRecoveryJournals();
  void windowClosed(EditorWindow *window);
  void report(const QString &message);

  DocumentStoreFactory m_stores;
  TextEditorRestorePolicy *m_policy;
  RestoreStateStore *m_restoreStore;
  RecoveryJournalStore *m_journalStore = nullptr;
  FileSelectionFactory m_choosers;
  DocumentDialogFactory m_dialogs;
  QList<QPointer<EditorWindow>> m_windows;
  QPointer<EditorWindow> m_activeWindow;
  std::optional<RestoreState> m_lastStored;
  bool m_showWindows;
  bool m_started = false;
  bool m_policyBaselineSeen = false;
  bool m_explicitPaths = false;
  bool m_restoreAttempted = false;
  bool m_admitting = false;
};
} // namespace QindaQt::Apps::TextEditor
