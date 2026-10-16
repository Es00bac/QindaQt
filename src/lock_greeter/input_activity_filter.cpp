// SPDX-License-Identifier: GPL-3.0-or-later
#include "input_activity_filter.h"
#include <QEvent>
#include <QKeyEvent>
namespace QindaQt::LockGreeter {
bool InputActivityFilter::eventFilter(QObject *, QEvent *event) {
  switch (event->type()) {
  case QEvent::KeyPress:
    if (static_cast<QKeyEvent *>(event)->key() != Qt::Key_Escape) Q_EMIT activity();
    break;
  case QEvent::MouseButtonPress:
  case QEvent::TouchBegin:
  case QEvent::TabletPress:
    Q_EMIT activity();
    break;
  default:
    break;
  }
  return false; // Observer only: input still reaches QML untouched.
}
} // namespace QindaQt::LockGreeter
