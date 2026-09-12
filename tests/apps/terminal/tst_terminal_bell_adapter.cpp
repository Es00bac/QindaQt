// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_appearance.h"
#include "ui/terminal_widget_adapter.h"

#include <qtermwidget.h>

#include <QFontDatabase>
#include <QTest>
#include <QWidget>

using namespace QindaQt::Apps::Terminal;

class TerminalBellAdapterTest final : public QObject {
  Q_OBJECT

private slots:
  void parsedAudibleBellTargetsOwningWindowWithoutStartingSession();
};

void TerminalBellAdapterTest::
    parsedAudibleBellTargetsOwningWindowWithoutStartingSession() {
  TerminalProfile profile = builtinDefaultProfile();
  profile.bellPolicy = TerminalProfile::BellPolicy::Audible;
  int beepCount = 0;
  int attentionCount = 0;
  QWidget *attentionTarget = nullptr;
  // The adapter owns and deletes its qtermwidget. Keep the borrowed parent
  // alive until after adapter teardown, matching TerminalWindow's member/base
  // destruction order.
  QWidget window;
  TerminalWidgetAdapter adapter(
      TerminalAppearanceAdapter::derive(
          QPalette(), TerminalContentScheme::Dark, false,
          QFontDatabase::systemFont(QFontDatabase::FixedFont)),
      profile,
      {
          .beep = [&beepCount] { ++beepCount; },
          .requestWindowAttention =
              [&attentionCount, &attentionTarget](QWidget *target) {
                ++attentionCount;
                attentionTarget = target;
              },
      });
  auto *widget = qobject_cast<QTermWidget *>(adapter.terminalWidget());
  QVERIFY(widget != nullptr);
  widget->setParent(&window);
  window.show();

  QVERIFY(QMetaObject::invokeMethod(widget, "bell", Qt::DirectConnection,
                                    Q_ARG(QString, QString{})));

  QCOMPARE(beepCount, 1);
  QCOMPARE(attentionCount, 1);
  QCOMPARE(attentionTarget, &window);
}

QTEST_MAIN(TerminalBellAdapterTest)
#include "tst_terminal_bell_adapter.moc"
