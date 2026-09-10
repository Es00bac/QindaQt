// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

class QClipboard;

namespace QindaQt::Apps::FileManager {
class MutationController;

// AGENT-CONTRACT: GUI-thread confined. Owns the File Manager clipboard policy
// only: which local paths are cut/copied, and where a paste or drop
// dispatches. All filesystem mutation goes through the injected
// MutationController's identity-checked batch contract; this object never
// touches the filesystem itself. The owned snapshot keeps the listing-time
// identity maps so paste of an own cut/copy is checked exactly like a
// context-menu batch; foreign clipboard content (another application owns the
// clipboard) is re-stat'ed at dispatch through copyForeignPathsTo and is
// never moved.
class ClipboardController final : public QObject {
  Q_OBJECT

  Q_PROPERTY(bool canPaste READ canPaste NOTIFY stateChanged FINAL)
  Q_PROPERTY(QString mode READ mode NOTIFY stateChanged FINAL)
  Q_PROPERTY(int count READ count NOTIFY stateChanged FINAL)
  Q_PROPERTY(int selectionCount READ selectionCount WRITE setSelectionCount
                 NOTIFY stateChanged FINAL)
  // Human-readable reason the last paste/drop refused to dispatch, for
  // surfaces that want more than a disabled action. Empty when the last
  // dispatch was accepted.
  Q_PROPERTY(QString lastRejection READ lastRejection NOTIFY stateChanged FINAL)

public:
  explicit ClipboardController(MutationController &mutation,
                               QClipboard &clipboard,
                               QObject *parent = nullptr);

  [[nodiscard]] bool canPaste() const;
  // "copy", "cut", or "none" (nothing pasteable on the clipboard).
  [[nodiscard]] QString mode() const;
  [[nodiscard]] int count() const;
  [[nodiscard]] int selectionCount() const { return m_selectionCount; }
  [[nodiscard]] QString lastRejection() const { return m_lastRejection; }

  // Items are the QML entry snapshots (path + decimal-string identity fields)
  // the mutation batch contract consumes.
  Q_INVOKABLE bool copySelection(const QVariantList &items);
  Q_INVOKABLE bool cutSelection(const QVariantList &items);
  Q_INVOKABLE bool pasteInto(const QString &destinationDirectory);
  // Foreign DnD payload: a text/uri-list. Local file URLs only, bounded by
  // MutationController::maximumForeignPaths; always copies, never moves.
  Q_INVOKABLE bool dropUrlsInto(const QVariantList &urls,
                                const QString &destinationDirectory);
  Q_INVOKABLE void clear();
  void setSelectionCount(int count);

  // Parses a text/uri-list payload into absolute local paths. Public and
  // static so drag targets and tests share exactly one acceptance rule.
  [[nodiscard]] static QStringList localPathsFromUrls(const QVariantList &urls);

signals:
  void stateChanged();

private:
  void adoptForeignClipboard();
  void publishSnapshot(bool cut);
  bool dispatchOwn(const QString &destinationDirectory);
  [[nodiscard]] bool reject(const QString &reason);

  MutationController &m_mutation;
  QClipboard &m_clipboard;
  QVariantList m_snapshot;
  bool m_cut = false;
  bool m_pendingCutPaste = false;
  QStringList m_foreignPaths;
  int m_selectionCount = 0;
  QString m_lastRejection;
};

} // namespace QindaQt::Apps::FileManager
