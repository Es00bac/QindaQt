// SPDX-License-Identifier: GPL-3.0-or-later
#include "launcher_test_support.h"

#include "qindaqt/shell_launcher/launcher_category_model.h"

#include <QTest>

using namespace QindaQt::ShellLauncher;
using namespace QindaQt::ShellLauncher::TestSupport;

class LauncherCategoryModelTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void mapsKnownXdgCategoriesToFixedGroups();
  void unmappedCategoriesLandInOther();
  void firstKnownCategoryWins();
  void groupingIsStableAndOrdered();
  void emptyEntriesProduceNoGroups();
};

void LauncherCategoryModelTest::mapsKnownXdgCategoriesToFixedGroups()
{
  QCOMPARE(LauncherCategoryModel::categoryFor({ QStringLiteral("Utility") }),
           LauncherCategory::Utilities);
  QCOMPARE(LauncherCategoryModel::categoryFor({ QStringLiteral("Development") }),
           LauncherCategory::Development);
  QCOMPARE(LauncherCategoryModel::categoryFor({ QStringLiteral("Game") }),
           LauncherCategory::Games);
  QCOMPARE(LauncherCategoryModel::categoryFor({ QStringLiteral("Graphics") }),
           LauncherCategory::Graphics);
  QCOMPARE(LauncherCategoryModel::categoryFor({ QStringLiteral("AudioVideo") }),
           LauncherCategory::AudioVideo);
  QCOMPARE(LauncherCategoryModel::categoryFor({ QStringLiteral("Music") }),
           LauncherCategory::AudioVideo);
  QCOMPARE(LauncherCategoryModel::categoryFor({ QStringLiteral("Network") }),
           LauncherCategory::Network);
  QCOMPARE(LauncherCategoryModel::categoryFor({ QStringLiteral("Office") }),
           LauncherCategory::Office);
  QCOMPARE(LauncherCategoryModel::categoryFor({ QStringLiteral("Science") }),
           LauncherCategory::Science);
  QCOMPARE(LauncherCategoryModel::categoryFor({ QStringLiteral("Education") }),
           LauncherCategory::Education);
  QCOMPARE(LauncherCategoryModel::categoryFor({ QStringLiteral("Settings") }),
           LauncherCategory::Settings);
  QCOMPARE(LauncherCategoryModel::categoryFor({ QStringLiteral("System") }),
           LauncherCategory::System);
}

void LauncherCategoryModelTest::unmappedCategoriesLandInOther()
{
  QCOMPARE(LauncherCategoryModel::categoryFor({ QStringLiteral("Custom") }),
           LauncherCategory::Other);
  QCOMPARE(LauncherCategoryModel::categoryFor({}), LauncherCategory::Other);
}

void LauncherCategoryModelTest::firstKnownCategoryWins()
{
  QCOMPARE(LauncherCategoryModel::categoryFor(
               { QStringLiteral("Custom"), QStringLiteral("Utility") }),
           LauncherCategory::Utilities);
  QCOMPARE(LauncherCategoryModel::categoryFor(
               { QStringLiteral("Utility"), QStringLiteral("System") }),
           LauncherCategory::Utilities);
}

void LauncherCategoryModelTest::groupingIsStableAndOrdered()
{
  const auto catalog = ApplicationCatalog::build(catalogCorpus());
  const auto groups = LauncherCategoryModel::group(catalog.entries());

  QVector<LauncherCategory> categories;
  for (const CategoryGroup &group : groups)
    categories.append(group.category);
  QCOMPARE(categories, (QVector<LauncherCategory> {
                         LauncherCategory::Graphics,
                         LauncherCategory::AudioVideo,
                         LauncherCategory::Network,
                         LauncherCategory::Office,
                         LauncherCategory::System,
                       }));

  const CategoryGroup &system = groups.last();
  QCOMPARE(system.entries.size(), 1);
  QCOMPARE(system.entries.first().name, QStringLiteral("Qinda Terminal"));

  // Identical input yields an identical grouping, byte for byte.
  const auto again = LauncherCategoryModel::group(catalog.entries());
  QCOMPARE(again, groups);
}

void LauncherCategoryModelTest::emptyEntriesProduceNoGroups()
{
  QVERIFY(LauncherCategoryModel::group({}).isEmpty());
}

QTEST_GUILESS_MAIN(LauncherCategoryModelTest)
#include "tst_launcher_category_model.moc"
