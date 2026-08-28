// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_appearance.h"

#include "qindaqt/themes/theme_loader.h"

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
  void schemeColorsNeverInventHexConstants();
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
    // The base and intense variants stay distinct so bright text is usable.
    QVERIFY(appearance.ansi[8] != appearance.ansi[0]);
    QVERIFY(appearance.ansi[15] != appearance.ansi[7]);
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
    for (int index = 0; index < 16; ++index) {
      const QString section =
          QStringLiteral("[Color%1]\n").arg(index);
      QVERIFY2(document.contains(section),
               qPrintable(QStringLiteral("%1 missing %2")
                              .arg(themeId, section)));
    }
    // Each section is followed by Color keys with r,g,b decimal triples.
    QVERIFY(document.contains(QLatin1String("Color=")));
    QVERIFY(!document.contains(QLatin1Char('#')));
  }
}

void TerminalAppearanceTest::schemeColorsNeverInventHexConstants() {
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

QTEST_MAIN(TerminalAppearanceTest)
#include "tst_terminal_appearance.moc"
