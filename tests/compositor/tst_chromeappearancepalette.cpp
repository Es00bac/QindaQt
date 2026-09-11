// SPDX-License-Identifier: GPL-3.0-or-later
#include "chromeappearancepalette.h"

#include "qindaqt/themes/theme_loader.h"

#include <QtTest>

#include <algorithm>
#include <cmath>

using namespace QindaQt::Compositor::KWinIntegration;

namespace {
double luminance(const QColor &color) {
  const auto linear = [](double channel) {
    channel /= 255.0;
    return channel <= 0.04045 ? channel / 12.92
                              : std::pow((channel + 0.055) / 1.055, 2.4);
  };
  return 0.2126 * linear(color.red()) + 0.7152 * linear(color.green()) +
         0.0722 * linear(color.blue());
}

double contrast(const QColor &left, const QColor &right) {
  const auto light = std::max(luminance(left), luminance(right));
  const auto dark = std::min(luminance(left), luminance(right));
  return (light + 0.05) / (dark + 0.05);
}
} // namespace

class ChromeAppearancePaletteTest final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void mapsThemeColors_data();
  void mapsThemeColors();
};

void ChromeAppearancePaletteTest::mapsThemeColors_data() {
  QTest::addColumn<QString>("themeId");
  QTest::newRow("light") << QStringLiteral("qinda-light");
  QTest::newRow("dark") << QStringLiteral("qinda-dark");
  QTest::newRow("high-contrast") << QStringLiteral("qinda-high-contrast");
}

void ChromeAppearancePaletteTest::mapsThemeColors() {
  QFETCH(QString, themeId);
  const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/") + themeId +
      QStringLiteral(".json"));
  QVERIFY2(loaded.ok, qPrintable(loaded.error));
  const auto palette = chromePaletteForTheme(loaded.theme);
  QString error;
  QVERIFY2(palette.isValid(&error), qPrintable(error));
  QCOMPARE(palette.surface,
           loaded.theme.colors.value(QStringLiteral("surface")));
  QCOMPARE(palette.text, loaded.theme.colors.value(QStringLiteral("text")));
  QVERIFY(contrast(palette.text, palette.surfaceRaised) >= 4.5);
  QVERIFY(contrast(palette.textMuted, palette.surface) >= 4.5);
  QCOMPARE(decorationPaletteProperties(palette, loaded.theme)
               .value(QStringLiteral("text"))
               .value<QColor>(),
           palette.text);
  const auto native = nativePaletteForTheme(loaded.theme);
  QCOMPARE(native.color(QPalette::Window),
           loaded.theme.colors.value(QStringLiteral("surface")));
  QCOMPARE(native.color(QPalette::Base),
           loaded.theme.colors.value(QStringLiteral("surfaceRaised")));
  QCOMPARE(native.color(QPalette::Text),
           loaded.theme.colors.value(QStringLiteral("text")));
  QCOMPARE(native.color(QPalette::Highlight),
           loaded.theme.colors.value(QStringLiteral("accent")));
  QCOMPARE(native.color(QPalette::HighlightedText),
           loaded.theme.colors.value(QStringLiteral("accentText")));
  QVERIFY(contrast(native.color(QPalette::Text),
                   native.color(QPalette::Base)) >= 4.5);
  QVERIFY(contrast(native.color(QPalette::HighlightedText),
                   native.color(QPalette::Highlight)) >= 4.5);
}

QTEST_MAIN(ChromeAppearancePaletteTest)
#include "tst_chromeappearancepalette.moc"
