// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_surface/desktop_icon_layout_store.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <limits>

using QindaQt::Shell::DesktopSurface::DesktopIconLayoutStore;

class DesktopIconLayoutStoreTests final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void persistsPerScreenCoordinatesAtomically();
  void clearScreenKeepsOtherOutputs();
  void malformedAndUnboundedDocumentsFailClosed();
};

void DesktopIconLayoutStoreTests::persistsPerScreenCoordinatesAtomically() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  const QString path = root.filePath(QStringLiteral("layout.json"));
  {
    DesktopIconLayoutStore store(path);
    QVERIFY(store.setPosition(QStringLiteral("DP-1"), QStringLiteral("1:42"),
                              123.5, 77.0));
  }
  DesktopIconLayoutStore restored(path);
  const QVariantMap point =
      restored.position(QStringLiteral("DP-1"), QStringLiteral("1:42"));
  QCOMPARE(point.value(QStringLiteral("x")).toReal(), 123.5);
  QCOMPARE(point.value(QStringLiteral("y")).toReal(), 77.0);
}

void DesktopIconLayoutStoreTests::clearScreenKeepsOtherOutputs() {
  QTemporaryDir root;
  DesktopIconLayoutStore store(root.filePath(QStringLiteral("layout.json")));
  QVERIFY(
      store.setPosition(QStringLiteral("DP-1"), QStringLiteral("1:1"), 1, 2));
  QVERIFY(store.setPosition(QStringLiteral("HDMI-A-1"), QStringLiteral("1:1"),
                            3, 4));
  QVERIFY(store.clearScreen(QStringLiteral("DP-1")));
  QVERIFY(
      store.position(QStringLiteral("DP-1"), QStringLiteral("1:1")).isEmpty());
  QCOMPARE(store.position(QStringLiteral("HDMI-A-1"), QStringLiteral("1:1"))
               .value(QStringLiteral("x"))
               .toInt(),
           3);
}

void DesktopIconLayoutStoreTests::malformedAndUnboundedDocumentsFailClosed() {
  QTemporaryDir root;
  const QString path = root.filePath(QStringLiteral("layout.json"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(
      R"({"schemaVersion":1,"screens":{"DP-1":{"1:2":{"x":"oops","y":4}}}})");
  file.close();
  DesktopIconLayoutStore store(path);
  QVERIFY(
      store.position(QStringLiteral("DP-1"), QStringLiteral("1:2")).isEmpty());
  QVERIFY(!store.setPosition(QStringLiteral("DP-1"),
                             QString(300, QLatin1Char('x')), 1, 2));
  QVERIFY(!store.setPosition(QStringLiteral("DP-1"), QStringLiteral("1:2"),
                             std::numeric_limits<qreal>::infinity(), 2));
}

QTEST_GUILESS_MAIN(DesktopIconLayoutStoreTests)
#include "tst_desktop_icon_layout_store.moc"
