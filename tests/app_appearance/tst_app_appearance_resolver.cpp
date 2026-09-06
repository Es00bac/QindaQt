// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include <qindaqt/app_appearance/appearance_resolver.h>

using namespace QindaQt::AppAppearance;
using QindaQt::Themes::ThemeSpec;

class AppAppearanceResolverTest final : public QObject {
  Q_OBJECT
private slots:
  void schemeControlsEffectiveTheme();
  void compatibleChoiceAndHighContrastSurvive();
  void missingAndUnknownAreDeterministic();
};

static ThemeSpec theme(QString id, QString variant) {
  ThemeSpec value;
  value.id = std::move(id);
  value.name = value.id;
  value.variant = std::move(variant);
  return value;
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

QTEST_GUILESS_MAIN(AppAppearanceResolverTest)
#include "tst_app_appearance_resolver.moc"
