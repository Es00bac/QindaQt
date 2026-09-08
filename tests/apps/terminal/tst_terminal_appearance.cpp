// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_appearance.h"

#include "qindaqt/themes/theme_loader.h"
#include "qindaqt/design_tokens/token_deriver.h"
#include "ui/terminal_ansi_palette.h"

#include <QTest>

using QindaQt::Apps::Terminal::TerminalAppearanceAdapter;
using QindaQt::Apps::Terminal::TerminalColorSchemeDocument;
using QindaQt::Apps::Terminal::TerminalViewAppearance;

namespace {

TerminalViewAppearance appearanceForTheme(const QString &themeId) {
  const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/") + themeId +
      QStringLiteral(".json"));
  if (!theme.ok) {
    qFatal("Could not load theme %s: %s", qPrintable(themeId),
           qPrintable(theme.error));
  }
  const auto appearance = TerminalAppearanceAdapter::fromTheme(theme.theme);
  if (!appearance.ok()) {
    qFatal("Could not derive appearance for %s: %s", qPrintable(themeId),
           qPrintable(appearance.diagnostic));
  }
  return *appearance.appearance;
}

} // namespace

class TerminalAppearanceTest final : public QObject {
  Q_OBJECT

private slots:
  void derivesCompleteAppearanceFromPublicTokens();
  void everyThemeDerivesAllSixteenAnsiSlots();
  void colorSchemeDocumentHasKonsoleSectionsForEveryTheme();
  void schemeColorsUseDecimalTriples();
  void customSurfaceKeepsReadableDistinctHues();
  void accessibilityInputsReachFontsAndContrast();
  void monospaceFontIsFixedPitchAndSized();
};

void TerminalAppearanceTest::derivesCompleteAppearanceFromPublicTokens() {
  const auto appearance = appearanceForTheme(QStringLiteral("qinda-dark"));
  QVERIFY(!appearance.sourceThemeId.isEmpty());
  QVERIFY(appearance.terminalBackground.isValid());
  QVERIFY(appearance.terminalForeground.isValid());
  QVERIFY(appearance.focusRing.isValid());
  QVERIFY(appearance.statusWarningForeground.isValid());
  QVERIFY(appearance.statusDangerForeground.isValid());
  // Window palette roles the presentation consumes directly.
  QVERIFY(appearance.windowPalette.color(QPalette::Window).isValid());
  QVERIFY(appearance.windowPalette.color(QPalette::Highlight).isValid());
}

void TerminalAppearanceTest::everyThemeDerivesAllSixteenAnsiSlots() {
  for (const QString &themeId :
       {QStringLiteral("qinda-light"), QStringLiteral("qinda-dusk"),
        QStringLiteral("qinda-dark"), QStringLiteral("qinda-high-contrast"),
        QStringLiteral("qinda-macos")}) {
    const auto appearance = appearanceForTheme(themeId);
    for (int index = 0; index < 16; ++index) {
      QVERIFY2(appearance.ansi[index].isValid(),
               qPrintable(QStringLiteral("%1 slot %2").arg(themeId)
                              .arg(index)));
    }
    using QindaQt::DesignTokens::DesignTokenDeriver;
    const double minimum = themeId == QStringLiteral("qinda-high-contrast") ? 7.0 : 4.5;
    for (int index = 0; index < 16; ++index) {
      QCOMPARE(appearance.ansi[index].alpha(), 255);
      QVERIFY2(DesignTokenDeriver::contrastRatio(appearance.ansi[index],
                    appearance.terminalBackground) >= minimum,
               qPrintable(QStringLiteral("%1 slot %2 lacks contrast").arg(themeId).arg(index)));
      if (index < 8)
        QVERIFY(appearance.ansi[index] != appearance.ansi[index + 8]);
    }
    QVERIFY(DesignTokenDeriver::contrastRatio(appearance.terminalForeground,
                 appearance.terminalBackground) >= minimum);
    QCOMPARE(appearance.terminalBackground.alpha(), 255);
    QVERIFY(DesignTokenDeriver::contrastRatio(
        appearance.windowPalette.color(QPalette::PlaceholderText),
        appearance.windowPalette.color(QPalette::Base)) >= 4.5);
    const QColor statusSurface = appearance.windowPalette.color(QPalette::Button);
    QVERIFY(DesignTokenDeriver::contrastRatio(appearance.statusWarningForeground, statusSurface) >= minimum);
    QVERIFY(DesignTokenDeriver::contrastRatio(appearance.statusDangerForeground, statusSurface) >= minimum);
  }
}

void TerminalAppearanceTest::
    colorSchemeDocumentHasKonsoleSectionsForEveryTheme() {
  for (const QString &themeId :
       {QStringLiteral("qinda-light"), QStringLiteral("qinda-dusk"),
        QStringLiteral("qinda-dark"), QStringLiteral("qinda-high-contrast"),
        QStringLiteral("qinda-macos")}) {
    const auto document =
        TerminalColorSchemeDocument::render(appearanceForTheme(themeId));
    QVERIFY(document.contains(QLatin1String("[Background]\n")));
    QVERIFY(document.contains(QLatin1String("[Foreground]\n")));
    for (int index = 0; index < 8; ++index) {
      const QString section =
          QStringLiteral("[Color%1]\n").arg(index);
      QVERIFY2(document.contains(section),
               qPrintable(QStringLiteral("%1 missing %2")
                              .arg(themeId, section)));
      const QString intenseSection =
          QStringLiteral("[Color%1Intense]\n").arg(index);
      QVERIFY2(document.contains(intenseSection),
               qPrintable(QStringLiteral("%1 missing %2")
                              .arg(themeId, intenseSection)));
    }
    QVERIFY(document.contains(
        QLatin1String("[General]\nDescription=QindaQt generated scheme\n")));
    QVERIFY(!document.contains(QLatin1String("[Color8]\n")));
    // Each section is followed by Color keys with r,g,b decimal triples.
    QVERIFY(document.contains(QLatin1String("Color=")));
    QVERIFY(!document.contains(QLatin1Char('#')));
  }
}

void TerminalAppearanceTest::schemeColorsUseDecimalTriples() {
  // Documents render decimal triples only; no hex literals can slip in.
  const auto document =
      TerminalColorSchemeDocument::render(
          appearanceForTheme(QStringLiteral("qinda-dark")));
  QVERIFY(!document.contains(QLatin1Char('#')));
  const auto lines = document.split(QLatin1Char('\n'));
  int colorKeys = 0;
  for (const QString &line : lines) {
    if (line.startsWith(QLatin1String("Color="))) {
      ++colorKeys;
      const auto parts = line.mid(6).split(QLatin1Char(','));
      QCOMPARE(parts.size(), 3);
      for (const QString &part : parts) {
        bool parsed = false;
        const int value = part.toInt(&parsed);
        QVERIFY(parsed);
        QVERIFY(value >= 0 && value <= 255);
      }
    }
  }
  QCOMPARE(colorKeys, 20); // Background/Foreground (+Intense) + 16 ANSI.
}

void TerminalAppearanceTest::monospaceFontIsFixedPitchAndSized() {
  const auto appearance = appearanceForTheme(QStringLiteral("qinda-dark"));
  QVERIFY(appearance.terminalFont.fixedPitch());
  QVERIFY(appearance.terminalFont.pointSizeF() > 0.0);
  QCOMPARE(appearance.terminalFont.styleHint(), QFont::Monospace);
  QVERIFY(appearance.interfaceFont.pointSizeF() > 0.0);
}

void TerminalAppearanceTest::customSurfaceKeepsReadableDistinctHues() {
  using namespace QindaQt::Apps::Terminal;
  using QindaQt::DesignTokens::DesignTokenDeriver;
  for (const QColor background : {QColor("#15292a"), QColor("#fff0dd"), QColor("#777777")}) {
    const auto palette = terminalAnsiPalette(background, false);
    for (int index = 0; index < 16; ++index)
      QVERIFY(DesignTokenDeriver::contrastRatio(palette[static_cast<std::size_t>(index)], background) >= 4.5);
    for (int index = 1; index <= 6; ++index) {
      QVERIFY(palette[static_cast<std::size_t>(index)] != palette[static_cast<std::size_t>(index + 8)]);
      // Distinct protocol hues must survive custom theme accent choices.
      for (int other = index + 1; other <= 6; ++other)
        QVERIFY(palette[static_cast<std::size_t>(index)] != palette[static_cast<std::size_t>(other)]);
    }
  }
}

void TerminalAppearanceTest::accessibilityInputsReachFontsAndContrast() {
  const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-light.json"));
  QVERIFY(theme.ok);
  QindaQt::DesignTokens::AccessibilityInputs inputs;
  inputs.basePointSize = 12;
  inputs.textScale = 1.5;
  inputs.highContrast = true;
  inputs.reducedTransparency = true;
  const auto adapted = TerminalAppearanceAdapter::fromTheme(theme.theme, inputs);
  QVERIFY(adapted.ok());
  QCOMPARE(adapted.appearance->terminalFont.pointSizeF(), 18.0);
  QCOMPARE(adapted.appearance->interfaceFont.pointSizeF(), 18.0);
  QVERIFY(QindaQt::DesignTokens::DesignTokenDeriver::contrastRatio(
      adapted.appearance->terminalForeground,
      adapted.appearance->terminalBackground) >= 7.0);
}

QTEST_MAIN(TerminalAppearanceTest)
#include "tst_terminal_appearance.moc"
