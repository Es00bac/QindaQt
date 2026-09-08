// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
class QWidget;

namespace QindaQt::Apps::TextEditor {
enum class DocumentCloseDecision { Save, Discard, Cancel };

// GUI-thread presentation seam. Each window owns its adapter; native adapters
// borrow that window as dialog parent. A cancelled operation never authorizes
// a write or close. Policy and persistence remain with their existing owners.
class DocumentDialogs {
public:
  virtual ~DocumentDialogs() = default;
  virtual DocumentCloseDecision confirmClose() = 0;
  virtual bool confirmReplace() = 0;
  virtual void operationError(const QString &title, const QString &message) = 0;
};

class NativeDocumentDialogs final : public DocumentDialogs {
public:
  explicit NativeDocumentDialogs(QWidget *parent);
  DocumentCloseDecision confirmClose() override;
  bool confirmReplace() override;
  void operationError(const QString &title, const QString &message) override;

private:
  QWidget *m_parent;
};
} // namespace QindaQt::Apps::TextEditor
