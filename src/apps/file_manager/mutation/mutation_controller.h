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
  // Batch variants run one serialized operation over a selection. Each item
  // map is the QML entry snapshot: a "path" string plus the decimal-string
  // identity fields identityFromMap() consumes. Every item still goes through
  // the backend's single-item, identity-checked contract, in order, inside
  // the one busy slot. The first typed failure stops the batch; cancellation
  // between items skips the remainder. Batch results carry no undo request
  // and no Trash restore token: undo stays one-level/single-item and
  // restoreLast() keeps referring to the most recent single-item trash.
  Q_INVOKABLE bool trashItems(const QVariantList &items);
  Q_INVOKABLE bool copyItemsTo(const QVariantList &items,
                               const QString &destinationDirectory);
  Q_INVOKABLE bool moveItemsTo(const QVariantList &items,
                               const QString &destinationDirectory);
  // Foreign payloads (another application's clipboard text/uri-list or DnD
  // URLs) carry no listing-time identity. Each source is stat'ed fresh at
  // dispatch and the backend re-verifies that identity before mutating, so
  // the identity-checked contract is preserved end to end. Batch semantics
  // match copyItemsTo/moveItemsTo (serialized, stop-on-first-failure, no
  // undo/restore tokens). Bounded to maximumForeignPaths per dispatch.
  static constexpr int maximumForeignPaths = 4096;
  Q_INVOKABLE bool copyForeignPathsTo(const QStringList &sourcePaths,
                                      const QString &destinationDirectory);
  Q_INVOKABLE bool moveForeignPathsTo(const QStringList &sourcePaths,
                                      const QString &destinationDirectory);
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
  // Validates the selection maps into per-item requests, then runs them in
  // order inside one worker invocation. Returns false (typed failure set)
  // when any item is invalid or another operation is running.
  [[nodiscard]] bool submitBatch(MutationKind kind, const QVariantList &items,
                                 const QString &destinationDirectory = {});
  // Resolves a fresh identity for each foreign source path and forwards the
  // resulting item maps to submitBatch.
  [[nodiscard]] bool submitForeignBatch(MutationKind kind,
                                        const QStringList &sourcePaths,
                                        const QString &destinationDirectory);
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
