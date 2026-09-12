// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_bell.h"

#include <QTest>
#include <QWidget>

using namespace QindaQt::Apps::Terminal;

class TerminalBellTest final : public QObject {
  Q_OBJECT

private slots:
  void audibleBellBeepsAndRequestsOwningWindowAttentionOnce();
  void silentBellDoesNothing();
  void repeatedAudibleBellsRemainOneDispatchPerBell();
  void hiddenWindowStillBeepsWithoutRequestingAttention();
};

void TerminalBellTest::audibleBellBeepsAndRequestsOwningWindowAttentionOnce() {
  QWidget window;
  QWidget terminalView(&window);
  window.show();
  int beepCount = 0;
  int attentionCount = 0;
  QWidget *attentionTarget = nullptr;
  const TerminalBellActions actions{
      .beep = [&beepCount] { ++beepCount; },
      .requestWindowAttention =
          [&attentionCount, &attentionTarget](QWidget *target) {
            ++attentionCount;
            attentionTarget = target;
          },
  };

  dispatchTerminalBell(TerminalProfile::BellPolicy::Audible, &terminalView,
                       actions);

  QCOMPARE(beepCount, 1);
  QCOMPARE(attentionCount, 1);
  QCOMPARE(attentionTarget, &window);
}

void TerminalBellTest::silentBellDoesNothing() {
  QWidget window;
  window.show();
  int beepCount = 0;
  int attentionCount = 0;
  const TerminalBellActions actions{
      .beep = [&beepCount] { ++beepCount; },
      .requestWindowAttention =
          [&attentionCount](QWidget *) { ++attentionCount; },
  };

  dispatchTerminalBell(TerminalProfile::BellPolicy::Silent, &window, actions);

  QCOMPARE(beepCount, 0);
  QCOMPARE(attentionCount, 0);
}

void TerminalBellTest::repeatedAudibleBellsRemainOneDispatchPerBell() {
  QWidget window;
  window.show();
  int beepCount = 0;
  int attentionCount = 0;
  const TerminalBellActions actions{
      .beep = [&beepCount] { ++beepCount; },
      .requestWindowAttention =
          [&attentionCount](QWidget *) { ++attentionCount; },
  };

  for (int bell = 0; bell < 3; ++bell) {
    dispatchTerminalBell(TerminalProfile::BellPolicy::Audible, &window,
                         actions);
  }

  QCOMPARE(beepCount, 3);
  QCOMPARE(attentionCount, 3);
}

void TerminalBellTest::hiddenWindowStillBeepsWithoutRequestingAttention() {
  QWidget window;
  int beepCount = 0;
  int attentionCount = 0;
  const TerminalBellActions actions{
      .beep = [&beepCount] { ++beepCount; },
      .requestWindowAttention =
          [&attentionCount](QWidget *) { ++attentionCount; },
  };

  dispatchTerminalBell(TerminalProfile::BellPolicy::Audible, &window, actions);

  QCOMPARE(beepCount, 1);
  QCOMPARE(attentionCount, 0);
}

QTEST_MAIN(TerminalBellTest)
#include "tst_terminal_bell.moc"
