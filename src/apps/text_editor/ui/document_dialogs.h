// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
class QWidget;

namespace QindaQt::Apps::TextEditor {
enum class DocumentCloseDecision { Save, Discard, Cancel };
enum class RecoveryDecision { Restore, Discard };

// GUI-thread presentation seam. Each window owns its adapter; native adapters
// borrow that window as dialog parent. A cancelled operation never authorizes
// a write or close, and recovery consent never silently restores: the user
// must explicitly choose Restore or Discard for journaled content. Policy and
// persistence remain with their existing owners.
class DocumentDialogs {
public:
  virtual ~DocumentDialogs() = default;
  virtual DocumentCloseDecision confirmClose() = 0;
  virtual bool confirmReplace() = 0;
  virtual void operationError(const QString &title, const QString &message) = 0;
  // displayName is the document title or "Untitled"; untitled selects the
  // wording for a buffer that has never been saved.
  virtual RecoveryDecision recoverUnsaved(const QString &displayName,
                                          bool untitled) = 0;
};

class NativeDocumentDialogs final : public DocumentDialogs {
public:
  explicit NativeDocumentDialogs(QWidget *parent);
  DocumentCloseDecision confirmClose() override;
  bool confirmReplace() override;
  void operationError(const QString &title, const QString &message) override;
  RecoveryDecision recoverUnsaved(const QString &displayName,
                                  bool untitled) override;

private:
  QWidget *m_parent;
};
} // namespace QindaQt::Apps::TextEditor
