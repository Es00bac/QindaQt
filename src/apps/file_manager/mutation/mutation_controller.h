// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mutation_backend.h"

#include <QObject>
#include <QThread>
#include <QVariantMap>

namespace QindaQt::Apps::FileManager {

// AGENT-CONTRACT: This object is GUI-thread confined and owns exactly one
// backend plus one in-flight worker task. Filesystem work never runs on the
// GUI thread. Destruction requests cancellation and joins the task before the
// backend is released. Results are never replayed, and one-level recovery is
// derived only from a successful, identity-bearing result.
class MutationController final : public QObject {
  Q_OBJECT

  Q_PROPERTY(bool busy READ busy NOTIFY stateChanged FINAL)
  Q_PROPERTY(int progressValue READ progressValue NOTIFY stateChanged FINAL)
  Q_PROPERTY(QString progressText READ progressText NOTIFY stateChanged FINAL)
  Q_PROPERTY(QString failureCode READ failureCode NOTIFY stateChanged FINAL)
  Q_PROPERTY(QString failureMessage READ failureMessage NOTIFY stateChanged FINAL)
  Q_PROPERTY(QString resultText READ resultText NOTIFY stateChanged FINAL)
  Q_PROPERTY(bool canUndo READ canUndo NOTIFY stateChanged FINAL)
  Q_PROPERTY(bool canRestore READ canRestore NOTIFY stateChanged FINAL)

public:
  explicit MutationController(MutationBackendPtr backend,
                              QObject *parent = nullptr);
  ~MutationController() override;

  [[nodiscard]] bool busy() const;
  [[nodiscard]] int progressValue() const;
  [[nodiscard]] QString progressText() const;
  [[nodiscard]] QString failureCode() const;
  [[nodiscard]] QString failureMessage() const;
  [[nodiscard]] QString resultText() const;
  [[nodiscard]] bool canUndo() const;
  [[nodiscard]] bool canRestore() const;

  Q_INVOKABLE bool createFolder(const QString &parentPath, const QString &name);
  Q_INVOKABLE bool renameItem(const QString &sourcePath, const QString &newName,
                              const QVariantMap &identity);
  Q_INVOKABLE bool copyItem(const QString &sourcePath,
                            const QString &destinationPath,
                            const QVariantMap &identity);
  Q_INVOKABLE bool moveItem(const QString &sourcePath,
                            const QString &destinationPath,
                            const QVariantMap &identity);
  Q_INVOKABLE bool trashItem(const QString &sourcePath,
                             const QVariantMap &identity);
  Q_INVOKABLE bool restoreLast();
  Q_INVOKABLE bool emptyTrash();
  Q_INVOKABLE bool undo();
  Q_INVOKABLE void cancel();
  Q_INVOKABLE void clearFailure();

signals:
  void stateChanged();
  void mutationCommitted();

private:
  [[nodiscard]] static std::optional<FileIdentity>
  identityFromMap(const QVariantMap &identity);
  [[nodiscard]] bool submit(MutationRequest request, bool isUndo = false);
  void finish(const MutationResult &result);
  void fail(MutationError error, const QString &message);

  MutationBackendPtr m_backend;
  QThread m_workerThread;
  QObject *m_workerContext = nullptr;
  MutationCancellation m_cancellation;
  std::shared_ptr<MutationRequest> m_undoRequest;
  QString m_lastTrashToken;
  QString m_lastTrashOriginalPath;
  std::optional<FileIdentity> m_lastTrashIdentity;
  int m_progressValue = 0;
  QString m_progressText;
  MutationError m_failure = MutationError::None;
  QString m_failureMessage;
  QString m_resultText;
  MutationKind m_runningKind = MutationKind::CreateFolder;
  bool m_isUndo = false;
  bool m_busy = false;
};

} // namespace QindaQt::Apps::FileManager
