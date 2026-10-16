// SPDX-License-Identifier: GPL-3.0-or-later
// The decision the Qt session-lock shell integration makes for every
// ext_session_lock_surface_v1.configure (ADR-0382). The integration plugin calls
// exactly this function, so the table below is the contract for what used to be
// a qFatal on a 0x0 configure.
#include "configure_policy.h"
#include <QtTest>
#include <climits>
using namespace QindaQt::LockProtocol;

class ConfigurePolicyTest : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void decision_data() {
    QTest::addColumn<unsigned>("width");
    QTest::addColumn<unsigned>("height");
    QTest::addColumn<bool>("usable");
    const unsigned beyond = static_cast<unsigned>(INT_MAX) + 1U;
    QTest::newRow("zero-by-zero") << 0U << 0U << false;
    QTest::newRow("zero-width") << 0U << 1080U << false;
    QTest::newRow("zero-height") << 1920U << 0U << false;
    QTest::newRow("width-beyond-qsize") << beyond << 1080U << false;
    QTest::newRow("height-beyond-qsize") << 1920U << beyond << false;
    QTest::newRow("everything-beyond") << UINT_MAX << UINT_MAX << false;
    QTest::newRow("one-by-one") << 1U << 1U << true;
    QTest::newRow("full-hd") << 1920U << 1080U << true;
    QTest::newRow("largest-qsize") << static_cast<unsigned>(INT_MAX) << static_cast<unsigned>(INT_MAX) << true;
  }
  void decision() {
    QFETCH(unsigned, width);
    QFETCH(unsigned, height);
    QFETCH(bool, usable);
    const auto size = usableConfigureSize(width, height);
    QCOMPARE(size.has_value(), usable);
    if (usable) {
      QCOMPARE(size->width(), static_cast<int>(width));
      QCOMPARE(size->height(), static_cast<int>(height));
    }
  }
};
QTEST_GUILESS_MAIN(ConfigurePolicyTest)
#include "tst_configure_policy.moc"
