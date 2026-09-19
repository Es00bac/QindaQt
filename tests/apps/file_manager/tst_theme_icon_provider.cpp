// SPDX-License-Identifier: GPL-3.0-or-later
#include "preview/theme_icon_provider.h"
#include <QIcon>
#include <QPalette>
#include <QtTest>
using namespace QindaQt::Apps::FileManager;
namespace {
constexpr int kEdge = 64;
// A glyph pixel counts only when it is essentially opaque: antialiased edges
// carry the tint color at low alpha and say nothing about the tint itself.
[[nodiscard]] bool paintsColor(const QPixmap &pixmap, const QColor &target,
                               int tolerance) {
  const QImage image = pixmap.toImage();
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      const QColor c = image.pixelColor(x, y);
      if (c.alpha() > 200 && qAbs(c.red() - target.red()) <= tolerance &&
          qAbs(c.green() - target.green()) <= tolerance &&
          qAbs(c.blue() - target.blue()) <= tolerance) {
        return true;
      }
    }
  }
  return false;
}
} // namespace
class ThemeIconProviderTest : public QObject {
  Q_OBJECT
private slots:
  void symbolicFollowsThePalette();
  void symbolicHonorsAnExplicitQueryColor();
  void symbolicRejectsAMalformedQueryColor();
  void fullColorIconsKeepTheirAuthoredPixels();
  void unresolvedNamesRenderTheColoredFallback();
  void unresolvedSymbolicNamesDoNotTintTheFallback();
};
void ThemeIconProviderTest::symbolicFollowsThePalette() {
  // The dark-theme contract: the authored #211D27 stroke would vanish on a
  // dark window, so the provider retints symbolic glyphs to the palette
  // foreground. Light themes keep working because WindowText is dark there.
  QPalette dark;
  dark.setColor(QPalette::Window, QColor(QStringLiteral("#26232B")));
  dark.setColor(QPalette::WindowText, QColor(QStringLiteral("#F4F1EA")));
  QGuiApplication::setPalette(dark);
  ThemeIconProvider provider;
  const QPixmap pixmap = provider.requestPixmap(
      QStringLiteral("go-previous-symbolic"), nullptr, QSize(kEdge, kEdge));
  QVERIFY(!pixmap.isNull());
  QVERIFY(paintsColor(pixmap, QColor(QStringLiteral("#F4F1EA")), 24));
  QVERIFY(!paintsColor(pixmap, QColor(QStringLiteral("#211D27")), 16));
}
void ThemeIconProviderTest::symbolicHonorsAnExplicitQueryColor() {
  // QML callers pass their palette color in the URL so a live theme switch
  // re-resolves the request; the provider trusts that color over the app
  // palette. "%23" is the URL-encoded '#'.
  ThemeIconProvider provider;
  const QPixmap pixmap = provider.requestPixmap(
      QStringLiteral("go-previous-symbolic?color=%23ff0000"), nullptr,
      QSize(kEdge, kEdge));
  QVERIFY(!pixmap.isNull());
  QVERIFY(paintsColor(pixmap, QColor(QStringLiteral("#ff0000")), 24));
}
void ThemeIconProviderTest::symbolicRejectsAMalformedQueryColor() {
  QPalette dark;
  dark.setColor(QPalette::WindowText, QColor(QStringLiteral("#F4F1EA")));
  QGuiApplication::setPalette(dark);
  ThemeIconProvider provider;
  const QPixmap pixmap = provider.requestPixmap(
      QStringLiteral("go-previous-symbolic?color=red"), nullptr,
      QSize(kEdge, kEdge));
  QVERIFY(!pixmap.isNull());
  QVERIFY(paintsColor(pixmap, QColor(QStringLiteral("#F4F1EA")), 24));
  QVERIFY(!paintsColor(pixmap, QColor(QStringLiteral("#ff0000")), 24));
}
void ThemeIconProviderTest::fullColorIconsKeepTheirAuthoredPixels() {
  // Only resolved "-symbolic" names recolor; pictograms keep their artwork.
  // A "?color=" query on a full-color name is ignored.
  ThemeIconProvider provider;
  const QPixmap pixmap = provider.requestPixmap(
      QStringLiteral("folder?color=%23ff0000"), nullptr, QSize(kEdge, kEdge));
  QVERIFY(!pixmap.isNull());
  QVERIFY(paintsColor(pixmap, QColor(QStringLiteral("#E8AE84")), 24));
  QVERIFY(!paintsColor(pixmap, QColor(QStringLiteral("#ff0000")), 16));
}
void ThemeIconProviderTest::unresolvedNamesRenderTheColoredFallback() {
  // The pre-existing fallback contract: an unknown name paints the theme's
  // full-color application-octet-stream document rather than a null pixmap
  // (a null result makes QML Image warn, fatal under the offscreen
  // QT_FATAL_WARNINGS rows).
  ThemeIconProvider provider;
  const QPixmap pixmap = provider.requestPixmap(
      QStringLiteral("no-such-icon-name"), nullptr, QSize(kEdge, kEdge));
  QVERIFY(!pixmap.isNull());
  QVERIFY(paintsColor(pixmap, QColor(QStringLiteral("#9C86AA")), 24));
}
void ThemeIconProviderTest::unresolvedSymbolicNamesDoNotTintTheFallback() {
  // A "-symbolic" name missing from the theme falls back to the colored
  // document; recoloring that would destroy full-color artwork, so the tint
  // must stay off and the query color must not appear.
  QPalette dark;
  dark.setColor(QPalette::WindowText, QColor(QStringLiteral("#00ff00")));
  QGuiApplication::setPalette(dark);
  ThemeIconProvider provider;
  const QPixmap pixmap = provider.requestPixmap(
      QStringLiteral("no-such-icon-symbolic?color=%23ff0000"), nullptr,
      QSize(kEdge, kEdge));
  QVERIFY(!pixmap.isNull());
  QVERIFY(paintsColor(pixmap, QColor(QStringLiteral("#9C86AA")), 24));
  QVERIFY(!paintsColor(pixmap, QColor(QStringLiteral("#00ff00")), 16));
  QVERIFY(!paintsColor(pixmap, QColor(QStringLiteral("#ff0000")), 16));
}
int main(int argc, char **argv) {
  QGuiApplication app(argc, argv);
  QIcon::setThemeSearchPaths(
      {QStringLiteral(QINDAQT_SOURCE_DIR) + QStringLiteral("/data/icons")});
  QIcon::setThemeName(QStringLiteral("QindaQt"));
  ThemeIconProviderTest test;
  return QTest::qExec(&test, argc, argv);
}
#include "tst_theme_icon_provider.moc"
