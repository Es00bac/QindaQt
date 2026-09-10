// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/panel_blur/panel_surface_blur.h"

#include <QtTest>

using QindaQt::PanelBlur::PanelSurfaceBlur;

class PanelBlurTests final : public QObject {
  Q_OBJECT

private slots:
  void emptyBoundsAndEmptyWindowsGiveEmptyRegions();
  void boundsAreClippedToTheWindowRect();
  void validBoundsProduceOneRect();
};

void PanelBlurTests::emptyBoundsAndEmptyWindowsGiveEmptyRegions() {
  QVERIFY(PanelSurfaceBlur::regionForBounds({}, QSize(400, 80)).isEmpty());
  QVERIFY(PanelSurfaceBlur::regionForBounds(QRectF(0, 0, 100, 40), QSize())
              .isEmpty());
}

void PanelBlurTests::boundsAreClippedToTheWindowRect() {
  // A magnified shelf may paint past the window edge; the blur region must
  // never request blur for pixels the surface does not cover.
  const QRegion region = PanelSurfaceBlur::regionForBounds(
      QRectF(-50, -10, 500, 100), QSize(400, 80));
  QCOMPARE(region.boundingRect(), QRect(0, 0, 400, 80));
}

void PanelBlurTests::validBoundsProduceOneRect() {
  const QRegion region =
      PanelSurfaceBlur::regionForBounds(QRectF(4, 4, 300, 60), QSize(400, 80));
  QCOMPARE(region.rectCount(), 1);
  QCOMPARE(region.boundingRect(), QRect(4, 4, 300, 60));
}

QTEST_GUILESS_MAIN(PanelBlurTests)
#include "tst_panel_blur.moc"
