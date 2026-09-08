// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "find/find_replace_engine.h"

#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace QindaQt::Apps::TextEditor {

class FindReplaceBar final : public QWidget {
  Q_OBJECT

public:
  explicit FindReplaceBar(QWidget *parent = nullptr);

  [[nodiscard]] FindOptions options() const;
  [[nodiscard]] QString replacement() const;
  [[nodiscard]] QLineEdit *findEditor() const;
  [[nodiscard]] QLineEdit *replaceEditor() const;
  void open(bool replaceMode);
  void closeBar();
  void setStatus(const QString &text, bool announce = true);

signals:
  void findNextRequested();
  void findPreviousRequested();
  void replaceRequested();
  void replaceAllRequested();
  void closed();

protected:
  void changeEvent(QEvent *event) override;

private:
  QWidget *m_replaceFields = nullptr;
  QLineEdit *m_find = nullptr;
  QLineEdit *m_replace = nullptr;
  QCheckBox *m_caseSensitive = nullptr;
  QCheckBox *m_wholeWord = nullptr;
  QCheckBox *m_regex = nullptr;
  QLabel *m_status = nullptr;
};

} // namespace QindaQt::Apps::TextEditor
