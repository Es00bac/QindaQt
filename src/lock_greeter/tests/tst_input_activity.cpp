// SPDX-License-Identifier: GPL-3.0-or-later
// The first key press, click or touch is what starts a PAM attempt (ADR-0382);
// nothing else may. Events are delivered with sendEvent, so this is deterministic.
#include "input_activity_filter.h"
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QSignalSpy>
#include <QTabletEvent>
#include <QTouchEvent>
#include <QtTest>
using namespace QindaQt::LockGreeter;

class InputActivityTest : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void keysClicksAndTouchesAreActivity() {
    QObject window;
    InputActivityFilter filter;
    window.installEventFilter(&filter);
    QSignalSpy activity(&filter, &InputActivityFilter::activity);
    QKeyEvent letter(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
    QCoreApplication::sendEvent(&window, &letter);
    QCOMPARE(activity.size(), 1);
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QCoreApplication::sendEvent(&window, &enter);
    QCOMPARE(activity.size(), 2);
    QKeyEvent shift(QEvent::KeyPress, Qt::Key_Shift, Qt::ShiftModifier); // Waking with a modifier counts.
    QCoreApplication::sendEvent(&window, &shift);
    QCOMPARE(activity.size(), 3);
    QMouseEvent click(QEvent::MouseButtonPress, QPointF(5, 5), QPointF(5, 5), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(&window, &click);
    QCOMPARE(activity.size(), 4);
    const QEventPoint point(1, QEventPoint::State::Pressed, QPointF(10, 10), QPointF(10, 10));
    QTouchEvent touch(QEvent::TouchBegin, QPointingDevice::primaryPointingDevice(), Qt::NoModifier, {point});
    QCoreApplication::sendEvent(&window, &touch);
    QCOMPARE(activity.size(), 5);
  }
  void escapeReleasesAndMotionAreNotActivity() {
    QObject window;
    InputActivityFilter filter;
    window.installEventFilter(&filter);
    QSignalSpy activity(&filter, &InputActivityFilter::activity);
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QCoreApplication::sendEvent(&window, &escape); // The cancel gesture never starts an attempt.
    QKeyEvent release(QEvent::KeyRelease, Qt::Key_A, Qt::NoModifier);
    QCoreApplication::sendEvent(&window, &release);
    QMouseEvent move(QEvent::MouseMove, QPointF(3, 3), QPointF(3, 3), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&window, &move); // A bumped mouse must not start PAM.
    QMouseEvent release2(QEvent::MouseButtonRelease, QPointF(3, 3), QPointF(3, 3), Qt::LeftButton, Qt::NoButton,
                         Qt::NoModifier);
    QCoreApplication::sendEvent(&window, &release2);
    QEvent other(QEvent::Enter);
    QCoreApplication::sendEvent(&window, &other);
    QCOMPARE(activity.size(), 0);
  }
  void theFilterNeverConsumesInput() {
    // A recording sibling filter installed AFTER ours (so it runs first) and one
    // BEFORE ours must both still see every event: ours returns false.
    class Recorder final : public QObject {
    public:
      bool eventFilter(QObject *, QEvent *event) override {
        if (event->type() == QEvent::KeyPress) ++keys;
        return false;
      }
      int keys = 0;
    };
    QObject window;
    InputActivityFilter filter;
    Recorder after, before;
    window.installEventFilter(&before);
    window.installEventFilter(&filter);
    window.installEventFilter(&after); // Filters run newest first: after, filter, before.
    QKeyEvent letter(QEvent::KeyPress, Qt::Key_B, Qt::NoModifier, QStringLiteral("b"));
    QCoreApplication::sendEvent(&window, &letter);
    QCOMPARE(after.keys, 1);
    QCOMPARE(before.keys, 1); // Reached only because our filter passed the event on.
  }
};
QTEST_MAIN(InputActivityTest)
#include "tst_input_activity.moc"
