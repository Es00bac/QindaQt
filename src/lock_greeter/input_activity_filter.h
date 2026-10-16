// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
namespace QindaQt::LockGreeter {
// Observes real input on a lock window and reports it as plain intent, so the
// controller can start a PAM attempt on the first key press, click or touch.
// It never consumes, alters or inspects event contents beyond their type and,
// for keys, whether they are Escape (the cancel gesture, never an attempt
// starter). Pointer motion is deliberately not activity: a bumped mouse must not
// start a conversation. Install on the window; GUI thread only.
class InputActivityFilter final : public QObject {
  Q_OBJECT
public:
  explicit InputActivityFilter(QObject *parent = nullptr) : QObject(parent) {}
Q_SIGNALS:
  void activity();
protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
};
} // namespace QindaQt::LockGreeter
