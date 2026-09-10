// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_appearance.h"
#include "ui/terminal_ansi_palette.h"

#include <QFontDatabase>
#include <QTest>

using QindaQt::Apps::Terminal::TerminalAppearanceAdapter;
using QindaQt::Apps::Terminal::TerminalColorSchemeDocument;
using QindaQt::Apps::Terminal::TerminalContentScheme;
using QindaQt::Apps::Terminal::TerminalViewAppearance;
using QindaQt::Apps::Terminal::isTerminalContentSchemeId;
using QindaQt::Apps::Terminal::terminalContentSchemeForId;
using QindaQt::Apps::Terminal::terminalContentSchemeId;
using QindaQt::Apps::Terminal::terminalContrastRatio;

namespace {

QFont monoFont() {
  return QFontDatabase::systemFont(QFontDatabase::FixedFont);
}

[[nodiscard]] QPalette paletteWith(QColor base, QColor text) {
  QPalette palette;
  palette.setColor(QPalette::Base, base);
  palette.setColor(QPalette::Text, text);
  return palette;
}

TerminalViewAppearance deriveFor(TerminalContentScheme scheme,
                                 bool highContrast = false,
                                 const QPalette &palette = QPalette()) {
  return TerminalAppearanceAdapter::derive(palette, scheme, highContrast,
                                           monoFont());
}

} // namespace

class TerminalAppearanceTest final : public QObject {
  Q_OBJECT

private slots:
  void systemSchemeDerivesContentFromTheActivePalette();
  void explicitSchemesPinContentSurfaces();
  void everySchemeDerivesAllSixteenAnsiSlots();
  void highContrastRaisesTheContrastFloor();
  void colorSchemeDocumentHasKonsoleSectionsForEveryScheme();
  void schemeColorsUseDecimalTriples();
  void customSurfaceKeepsReadableDistinctHues();
  void monospaceFontIsFixedPitchAndSized();
  void schemeIdVocabularyRoundTripsAndMapsLegacyThemes();
};

void TerminalAppearanceTest::systemSchemeDerivesContentFromTheActivePalette() {
  // ADR-0116: content truth is the live application palette, not QST tokens.
  const QPalette lightish = paletteWith(QColor("#f5f0ec"), QColor("#241f28"));
  const auto lightAppearance =
      TerminalAppearanceAdapter::derive(lightish, TerminalContentScheme::System,
                                        false, monoFont());
  QCOMPARE(lightAppearance.terminalBackground,
           lightish.color(QPalette::Base));
  QCOMPARE(lightAppearance.terminalForeground,
           lightish.color(QPalette::Text));
  QCOMPARE(lightAppearance.schemeId, QStringLiteral("system"));

  const QPalette darkish = paletteWith(QColor("#1d1a22"), QColor("#f2ecf1"));
  const auto darkAppearance =
      TerminalAppearanceAdapter::derive(darkish, TerminalContentScheme::System,
                                        false, monoFont());
  QCOMPARE(darkAppearance.terminalBackground, darkish.color(QPalette::Base));
  QCOMPARE(darkAppearance.terminalForeground,
           darkish.color(QPalette::Text));
  QVERIFY(lightAppearance.terminalBackground != darkAppearance.terminalBackground);

  // A low-contrast incoming pair is fitted, never inherited verbatim.
  const QPalette weak = paletteWith(QColor("#777777"), QColor("#787878"));
  const auto fitted = TerminalAppearanceAdapter::derive(
      weak, TerminalContentScheme::System, false, monoFont());
  QVERIFY(terminalContrastRatio(fitted.terminalForeground,
                                fitted.terminalBackground) >= 4.5);
  QCOMPARE(fitted.terminalBackground.alpha(), 255);
}

void TerminalAppearanceTest::explicitSchemesPinContentSurfaces() {
  // A profile's explicit scheme pins the content surface regardless of the
  // desktop palette the caller's palette carries.
  const QPalette darkish = paletteWith(QColor("#1d1a22"), QColor("#f2ecf1"));
  const auto light = TerminalAppearanceAdapter::derive(
      darkish, TerminalContentScheme::Light, false, monoFont());
  QCOMPARE(light.terminalBackground, QColor(Qt::white));
  QCOMPARE(light.schemeId, QStringLiteral("light"));
  const auto dark = TerminalAppearanceAdapter::derive(
      QPalette(), TerminalContentScheme::Dark, false, monoFont());
  QCOMPARE(dark.terminalBackground, QColor(Qt::black));
  QCOMPARE(dark.schemeId, QStringLiteral("dark"));
  QVERIFY(terminalContrastRatio(light.terminalForeground,
                                light.terminalBackground) >= 4.5);
  QVERIFY(terminalContrastRatio(dark.terminalForeground,
                                dark.terminalBackground) >= 4.5);
}

void TerminalAppearanceTest::everySchemeDerivesAllSixteenAnsiSlots() {
  for (const TerminalContentScheme scheme :
       {TerminalContentScheme::System, TerminalContentScheme::Light,
        TerminalContentScheme::Dark}) {
    const auto appearance = deriveFor(scheme);
    for (int index = 0; index < 16; ++index) {
      const QString label =
          QStringLiteral("%1 slot %2").arg(appearance.schemeId).arg(index);
      QVERIFY2(appearance.ansi[index].isValid(), qPrintable(label));
      QCOMPARE(appearance.ansi[index].alpha(), 255);
      QVERIFY2(terminalContrastRatio(appearance.ansi[index],
                                     appearance.terminalBackground) >= 4.5,
               qPrintable(label));
      if (index < 8)
        QVERIFY(appearance.ansi[index] != appearance.ansi[index + 8]);
    }
    QVERIFY(terminalContrastRatio(appearance.terminalForeground,
                                  appearance.terminalBackground) >= 4.5);
    QCOMPARE(appearance.terminalBackground.alpha(), 255);
  }
}

void TerminalAppearanceTest::highContrastRaisesTheContrastFloor() {
  for (const TerminalContentScheme scheme :
       {TerminalContentScheme::System, TerminalContentScheme::Light,
        TerminalContentScheme::Dark}) {
    const auto appearance = deriveFor(scheme, true);
    QVERIFY(appearance.highContrast);
    for (int index = 0; index < 16; ++index) {
      QVERIFY2(terminalContrastRatio(appearance.ansi[index],
                                     appearance.terminalBackground) >= 7.0,
               qPrintable(QStringLiteral("%1 slot %2 lacks 7:1")
                              .arg(appearance.schemeId)
                              .arg(index)));
    }
    QVERIFY(terminalContrastRatio(appearance.terminalForeground,
                                  appearance.terminalBackground) >= 7.0);
  }
}

void TerminalAppearanceTest::
    colorSchemeDocumentHasKonsoleSectionsForEveryScheme() {
  for (const TerminalContentScheme scheme :
       {TerminalContentScheme::System, TerminalContentScheme::Light,
        TerminalContentScheme::Dark}) {
    const auto document = TerminalColorSchemeDocument::render(deriveFor(scheme));
    QVERIFY(document.contains(QLatin1String("[Background]\n")));
    QVERIFY(document.contains(QLatin1String("[Foreground]\n")));
    for (int index = 0; index < 8; ++index) {
      const QString section = QStringLiteral("[Color%1]\n").arg(index);
      QVERIFY2(document.contains(section),
               qPrintable(QStringLiteral("scheme missing %1").arg(section)));
      const QString intenseSection =
          QStringLiteral("[Color%1Intense]\n").arg(index);
      QVERIFY2(document.contains(intenseSection),
               qPrintable(QStringLiteral("scheme missing %1").arg(intenseSection)));
    }
    QVERIFY(document.contains(
        QLatin1String("[General]\nDescription=QindaQt generated scheme\n")));
    QVERIFY(!document.contains(QLatin1String("[Color8]\n")));
    QVERIFY(document.contains(QLatin1String("Color=")));
    QVERIFY(!document.contains(QLatin1Char('#')));
  }
}

void TerminalAppearanceTest::schemeColorsUseDecimalTriples() {
  // Documents render decimal triples only; no hex literals can slip in.
  const auto document = TerminalColorSchemeDocument::render(
      deriveFor(TerminalContentScheme::Dark));
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

void TerminalAppearanceTest::customSurfaceKeepsReadableDistinctHues() {
  using namespace QindaQt::Apps::Terminal;
  for (const QColor background :
       {QColor("#15292a"), QColor("#fff0dd"), QColor("#777777")}) {
    const auto palette = terminalAnsiPalette(background, false);
    for (int index = 0; index < 16; ++index)
      QVERIFY(terminalContrastRatio(palette[static_cast<std::size_t>(index)],
                                    background) >= 4.5);
    for (int index = 1; index <= 6; ++index) {
      QVERIFY(palette[static_cast<std::size_t>(index)] !=
              palette[static_cast<std::size_t>(index + 8)]);
      // Distinct protocol hues must survive custom surface choices.
      for (int other = index + 1; other <= 6; ++other)
        QVERIFY(palette[static_cast<std::size_t>(index)] !=
                palette[static_cast<std::size_t>(other)]);
    }
  }
}

void TerminalAppearanceTest::monospaceFontIsFixedPitchAndSized() {
  const auto appearance = deriveFor(TerminalContentScheme::System);
  QVERIFY(appearance.terminalFont.fixedPitch());
  QVERIFY(appearance.terminalFont.pointSizeF() > 0.0);
  QCOMPARE(appearance.terminalFont.styleHint(), QFont::Monospace);
  // The base size is the platform FixedFont's, not a per-app token's.
  QCOMPARE(appearance.terminalFont.pointSizeF(), monoFont().pointSizeF());
}

void TerminalAppearanceTest::schemeIdVocabularyRoundTripsAndMapsLegacyThemes() {
  QVERIFY(isTerminalContentSchemeId(QStringLiteral("system")));
  QVERIFY(isTerminalContentSchemeId(QStringLiteral("light")));
  QVERIFY(isTerminalContentSchemeId(QStringLiteral("dark")));
  QVERIFY(!isTerminalContentSchemeId(QStringLiteral("qinda-dark")));
  QVERIFY(!isTerminalContentSchemeId(QStringLiteral("../../theme")));
  QVERIFY(!isTerminalContentSchemeId(QString()));

  QCOMPARE(terminalContentSchemeId(TerminalContentScheme::System),
           QStringLiteral("system"));
  QCOMPARE(terminalContentSchemeId(TerminalContentScheme::Light),
           QStringLiteral("light"));
  QCOMPARE(terminalContentSchemeId(TerminalContentScheme::Dark),
           QStringLiteral("dark"));
  QCOMPARE(terminalContentSchemeForId(QStringLiteral("system")),
           std::optional(TerminalContentScheme::System));
  QCOMPARE(terminalContentSchemeForId(QStringLiteral("light")),
           std::optional(TerminalContentScheme::Light));
  QCOMPARE(terminalContentSchemeForId(QStringLiteral("dark")),
           std::optional(TerminalContentScheme::Dark));

  // Pre-ADR-0116 persisted profiles carry QST theme ids; they map by variant.
  QCOMPARE(terminalContentSchemeForId(QStringLiteral("qinda-dark")),
           std::optional(TerminalContentScheme::Dark));
  QCOMPARE(terminalContentSchemeForId(QStringLiteral("qinda-dusk")),
           std::optional(TerminalContentScheme::Dark));
  QCOMPARE(terminalContentSchemeForId(QStringLiteral("qinda-high-contrast")),
           std::optional(TerminalContentScheme::Dark));
  QCOMPARE(terminalContentSchemeForId(QStringLiteral("qinda-light")),
           std::optional(TerminalContentScheme::Light));
  QCOMPARE(terminalContentSchemeForId(QStringLiteral("qinda-macos")),
           std::optional(TerminalContentScheme::Light));
  QCOMPARE(terminalContentSchemeForId(QStringLiteral("unknown-id")),
           std::nullopt);
}

QTEST_MAIN(TerminalAppearanceTest)
#include "tst_terminal_appearance.moc"
