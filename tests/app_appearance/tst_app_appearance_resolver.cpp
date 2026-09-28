// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include <qindaqt/app_appearance/appearance_resolver.h>
#include <qindaqt/themes/theme_loader.h>

#include <utility>

using namespace QindaQt::AppAppearance;
using QindaQt::Themes::ThemeSpec;

class AppAppearanceResolverTest final : public QObject {
  Q_OBJECT
private slots:
  void schemeControlsEffectiveTheme();
  void compatibleChoiceAndHighContrastSurvive();
  void missingAndUnknownAreDeterministic();
  void pairedThemesFollowTheSchemeToTheirTwin();
  void missingOrMismatchedTwinFallsBackAsBefore();
  void shippedCornerBarThemesPairBothWays();
};

static ThemeSpec theme(QString id, QString variant) {
  ThemeSpec value;
  value.id = std::move(id);
  value.name = value.id;
  value.variant = std::move(variant);
  return value;
}

static ThemeSpec paired(QString id, QString variant, QString light,
                        QString dark) {
  ThemeSpec value = theme(std::move(id), std::move(variant));
  value.lightVariant = std::move(light);
  value.darkVariant = std::move(dark);
  return value;
}

static QString resolved(const QVector<ThemeSpec> &themes, const QString &id,
                        ColorSchemePreference scheme, Qt::ColorScheme platform) {
  const auto result = resolveAppearanceTheme(themes, {id, scheme}, platform);
  return result ? result->id : QStringLiteral("<none>");
}

void AppAppearanceResolverTest::schemeControlsEffectiveTheme() {
  const QVector themes{theme("qinda-light", "light"),
                       theme("qinda-dark", "dark"),
                       theme("qinda-dusk", "dusk")};
  QCOMPARE(resolveAppearanceTheme(themes,
                                  {"qinda-dark", ColorSchemePreference::Light},
                                  Qt::ColorScheme::Dark)
               ->id,
           "qinda-light");
  QCOMPARE(resolveAppearanceTheme(themes,
                                  {"qinda-light", ColorSchemePreference::Dark},
                                  Qt::ColorScheme::Light)
               ->id,
           "qinda-dark");
  QCOMPARE(resolveAppearanceTheme(
               themes, {"qinda-light", ColorSchemePreference::System},
               Qt::ColorScheme::Dark)
               ->id,
           "qinda-light");
  QCOMPARE(resolveAppearanceTheme(themes,
                                  {"qinda-dark", ColorSchemePreference::System},
                                  Qt::ColorScheme::Light)
               ->id,
           "qinda-dark");
}

void AppAppearanceResolverTest::compatibleChoiceAndHighContrastSurvive() {
  const QVector themes{theme("qinda-light", "light"),
                       theme("qinda-dark", "dark"), theme("qinda-dusk", "dusk"),
                       theme("qinda-high-contrast", "high-contrast")};
  QCOMPARE(resolveAppearanceTheme(themes,
                                  {"qinda-dusk", ColorSchemePreference::Dark},
                                  Qt::ColorScheme::Light)
               ->id,
           "qinda-dusk");
  QCOMPARE(resolveAppearanceTheme(
               themes, {"qinda-high-contrast", ColorSchemePreference::Light},
               Qt::ColorScheme::Light)
               ->id,
           "qinda-high-contrast");
}

void AppAppearanceResolverTest::missingAndUnknownAreDeterministic() {
  const QVector themes{theme("qinda-light", "light"),
                       theme("qinda-dark", "dark")};
  QCOMPARE(resolveAppearanceTheme(themes,
                                  {"missing", ColorSchemePreference::System},
                                  Qt::ColorScheme::Unknown)
               ->id,
           "qinda-dark");
  QVERIFY(!colorSchemeFromToken("sepia"));
  QCOMPARE(colorSchemeToken(ColorSchemePreference::Light), "light");
}

// ADR-0284: a theme that authors `variants` resolves to its twin whenever the
// effective scheme (forced, or the platform's while following the system)
// wants the other one, in both directions.
void AppAppearanceResolverTest::pairedThemesFollowTheSchemeToTheirTwin() {
  const QVector themes{
      theme("qinda-light", "light"), theme("qinda-dark", "dark"),
      paired("qinda-marigold", "light", "qinda-marigold", "qinda-marigold-dark"),
      paired("qinda-marigold-dark", "dark", "qinda-marigold",
             "qinda-marigold-dark")};
  using S = ColorSchemePreference;
  // Forced schemes.
  QCOMPARE(resolved(themes, "qinda-marigold", S::Dark, Qt::ColorScheme::Light),
           "qinda-marigold-dark");
  QCOMPARE(resolved(themes, "qinda-marigold-dark", S::Light, Qt::ColorScheme::Dark),
           "qinda-marigold");
  QCOMPARE(resolved(themes, "qinda-marigold", S::Light, Qt::ColorScheme::Dark),
           "qinda-marigold");
  // Following the system (the portal's dark preference arrives this way).
  QCOMPARE(resolved(themes, "qinda-marigold", S::System, Qt::ColorScheme::Dark),
           "qinda-marigold-dark");
  QCOMPARE(resolved(themes, "qinda-marigold-dark", S::System, Qt::ColorScheme::Light),
           "qinda-marigold");
  QCOMPARE(resolved(themes, "qinda-marigold", S::System, Qt::ColorScheme::Light),
           "qinda-marigold");
  // An unknown platform scheme keeps the selection exactly as chosen.
  QCOMPARE(resolved(themes, "qinda-marigold-dark", S::System, Qt::ColorScheme::Unknown),
           "qinda-marigold-dark");
  // Unpaired themes behave as they always did.
  QCOMPARE(resolved(themes, "qinda-light", S::System, Qt::ColorScheme::Dark),
           "qinda-light");
  QCOMPARE(resolved(themes, "qinda-light", S::Dark, Qt::ColorScheme::Light),
           "qinda-dark");
}

void AppAppearanceResolverTest::missingOrMismatchedTwinFallsBackAsBefore() {
  using S = ColorSchemePreference;
  // The twin is named but not installed: forced dark falls back to the
  // built-in, following the system keeps the selection.
  const QVector missing{
      theme("qinda-light", "light"), theme("qinda-dark", "dark"),
      paired("qinda-marigold", "light", "qinda-marigold", "qinda-marigold-dark")};
  QCOMPARE(resolved(missing, "qinda-marigold", S::Dark, Qt::ColorScheme::Light),
           "qinda-dark");
  QCOMPARE(resolved(missing, "qinda-marigold", S::System, Qt::ColorScheme::Dark),
           "qinda-marigold");
  // A twin that is not really of the wanted scheme is ignored.
  const QVector mislabelled{
      theme("qinda-light", "light"), theme("qinda-dark", "dark"),
      paired("qinda-marigold", "light", "", "qinda-teal"),
      theme("qinda-teal", "light")};
  QCOMPARE(resolved(mislabelled, "qinda-marigold", S::Dark, Qt::ColorScheme::Dark),
           "qinda-dark");
  // No pair at all: exactly today's behaviour.
  const QVector unpaired{theme("qinda-light", "light"), theme("qinda-dark", "dark"),
                         theme("qinda-marigold", "light")};
  QCOMPARE(resolved(unpaired, "qinda-marigold", S::Dark, Qt::ColorScheme::Dark),
           "qinda-dark");
  QCOMPARE(resolved(unpaired, "qinda-marigold", S::System, Qt::ColorScheme::Dark),
           "qinda-marigold");
}

void AppAppearanceResolverTest::shippedCornerBarThemesPairBothWays() {
  QVector<ThemeSpec> themes;
  for (const auto &result : QindaQt::Themes::ThemeLoader::fromDirectory(
           QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes"))) {
    QVERIFY2(result.ok, qPrintable(result.error));
    themes.append(result.theme);
  }
  using S = ColorSchemePreference;
  for (const auto &[light, dark] :
       {std::pair{"qinda-marigold", "qinda-marigold-dark"},
        std::pair{"qinda-corner-teal", "qinda-corner-teal-dark"},
        std::pair{"qinda-corner-violet", "qinda-corner-violet-dark"}}) {
    QCOMPARE(resolved(themes, light, S::Dark, Qt::ColorScheme::Light), dark);
    QCOMPARE(resolved(themes, light, S::System, Qt::ColorScheme::Dark), dark);
    QCOMPARE(resolved(themes, dark, S::Light, Qt::ColorScheme::Dark), light);
    QCOMPARE(resolved(themes, dark, S::System, Qt::ColorScheme::Light), light);
  }
}

QTEST_GUILESS_MAIN(AppAppearanceResolverTest)
#include "tst_app_appearance_resolver.moc"
