// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QString>
namespace QindaQt::Power {
// Same-thread, borrowed asynchronous notification seam. One outstanding owned
// countdown only; close serializes behind any update. Confirmed closure is a
// prerequisite for action dispatch. Failure never authorizes an action.
class CriticalNotification : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void show(const QString &action, int seconds) = 0;
    virtual void update(int seconds) = 0;
    virtual void close() = 0;
Q_SIGNALS:
    void shown();
    void cancelled();
    void closed();
    void failed();
};
}
