// SPDX-License-Identifier: GPL-3.0-or-later
#include <QWindow>
#include <QtTest>
#include <qindaqt/application_window_management/client.h>
using namespace QindaQt::ApplicationWindowManagement;
class ClientTests : public QObject {
  Q_OBJECT
private slots:
  void unsupportedPlatformPreservesOrdinaryWindows() {
    WindowPlacementClient client;
    QVERIFY(!client.available());
    QWindow source, created;
    QString error;
    QCOMPARE(client.place(&source, &created, Placement::Tab, &error),
             quint32(0));
    QVERIFY(!error.isEmpty());
    QVERIFY(!source.isVisible());
    QVERIFY(!created.isVisible());
    client.cancel(42);
  }
};
QTEST_MAIN(ClientTests)
#include "tst_client.moc"
