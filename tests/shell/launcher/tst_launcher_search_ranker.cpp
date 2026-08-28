// SPDX-License-Identifier: GPL-3.0-or-later
#include "launcher_test_support.h"

#include "qindaqt/shell_launcher/launcher_bounds.h"
#include "qindaqt/shell_launcher/launcher_search_ranker.h"

#include <QTest>

using namespace QindaQt::ShellLauncher;
using namespace QindaQt::ShellLauncher::TestSupport;

namespace {

QVector<QString> rankedNames(const SearchOutcome &outcome)
{
  QVector<QString> names;
  for (const RankedEntry &ranked : outcome.results)
    names.append(ranked.entry.name);
  return names;
}

} // namespace

class LauncherSearchRankerTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void prefixBeatsWordStartBeatsSubstring();
  void keywordsRankBelowNameMatches();
  void genericNameAndCommentAreLastResortMatches();
  void tiesBreakByNameThenId();
  void queryNormalizationIsWhitespaceAndCaseInsensitive();
  void emptyAndBlankQueriesAreErrors();
  void oversizeQueryIsRejected();
  void noMatchYieldsEmptyOkOutcome();
  void matchKindEnumOrderIsTheScoreOrder();
};

void LauncherSearchRankerTest::prefixBeatsWordStartBeatsSubstring()
{
  const auto catalog = ApplicationCatalog::build(catalogCorpus());

  // "qin" prefixes both Qinda names; the editor wins on the name tie-break.
  const auto prefix =
      LauncherSearchRanker::search(catalog.entries(), QStringLiteral("qin"));
  QVERIFY(prefix.ok());
  QCOMPARE(prefix.results.size(), 2);
  QCOMPARE(prefix.results.first().match, SearchMatchKind::NamePrefix);
  QCOMPARE(prefix.results.first().entry.name, QStringLiteral("Qinda Editor"));

  // "viewer" only word-starts and substrings within "Image Viewer".
  const auto wordStart =
      LauncherSearchRanker::search(catalog.entries(), QStringLiteral("viewer"));
  QVERIFY(wordStart.ok());
  QCOMPARE(wordStart.results.first().match, SearchMatchKind::NameWordStart);

  // "mage" only substrings inside "Image Viewer".
  const auto substring =
      LauncherSearchRanker::search(catalog.entries(), QStringLiteral("mage"));
  QVERIFY(substring.ok());
  QCOMPARE(substring.results.first().match, SearchMatchKind::NameSubstring);
}

void LauncherSearchRankerTest::keywordsRankBelowNameMatches()
{
  EntryTemplate entry;
  entry.name = QStringLiteral("Zeta Reader");
  entry.keywords = QStringLiteral("browser;");
  const auto catalog = ApplicationCatalog::build(
      { document(QStringLiteral("app.zeta"), entry), catalogCorpus().last() });

  const auto outcome =
      LauncherSearchRanker::search(catalog.entries(), QStringLiteral("browser"));
  QVERIFY(outcome.ok());
  // "Web Browser" matches the name substring; the exact keyword match ranks
  // below every name match even though it is a keyword prefix.
  QCOMPARE(rankedNames(outcome),
           (QVector<QString> { QStringLiteral("Web Browser"),
                               QStringLiteral("Zeta Reader") }));
  QCOMPARE(outcome.results.last().match, SearchMatchKind::KeywordPrefix);
}

void LauncherSearchRankerTest::genericNameAndCommentAreLastResortMatches()
{
  const auto catalog = ApplicationCatalog::build(catalogCorpus());

  const auto generic =
      LauncherSearchRanker::search(catalog.entries(), QStringLiteral("emulator"));
  QVERIFY(generic.ok());
  QCOMPARE(generic.results.size(), 1);
  QCOMPARE(generic.results.first().entry.id, QStringLiteral("org.qinda.terminal"));
  QCOMPARE(generic.results.first().match, SearchMatchKind::GenericNameSubstring);

  const auto comment =
      LauncherSearchRanker::search(catalog.entries(), QStringLiteral("pictures"));
  QVERIFY(comment.ok());
  QCOMPARE(comment.results.size(), 1);
  QCOMPARE(comment.results.first().entry.id, QStringLiteral("org.qinda.viewer"));
  QCOMPARE(comment.results.first().match, SearchMatchKind::CommentSubstring);
}

void LauncherSearchRankerTest::tiesBreakByNameThenId()
{
  auto first = EntryTemplate {};
  first.name = QStringLiteral("Alpha Tool");
  auto second = EntryTemplate {};
  second.name = QStringLiteral("alpha time");
  const auto catalog = ApplicationCatalog::build(
      { document(QStringLiteral("app.b"), second),
        document(QStringLiteral("app.a"), first) });

  const auto outcome =
      LauncherSearchRanker::search(catalog.entries(), QStringLiteral("alpha"));
  QVERIFY(outcome.ok());
  // Case-insensitive folding puts "alpha time" (i) before "Alpha Tool" (o);
  // the input order app.b before app.a must not matter.
  QCOMPARE(rankedNames(outcome),
           (QVector<QString> { QStringLiteral("alpha time"),
                               QStringLiteral("Alpha Tool") }));
}

void LauncherSearchRankerTest::queryNormalizationIsWhitespaceAndCaseInsensitive()
{
  const auto catalog = ApplicationCatalog::build(catalogCorpus());

  const auto outcome = LauncherSearchRanker::search(
      catalog.entries(), QStringLiteral("  QINDA\t\tEDITOR  "));
  QVERIFY(outcome.ok());
  QCOMPARE(rankedNames(outcome), QVector<QString> { QStringLiteral("Qinda Editor") });
}

void LauncherSearchRankerTest::emptyAndBlankQueriesAreErrors()
{
  const auto catalog = ApplicationCatalog::build(catalogCorpus());

  const auto empty = LauncherSearchRanker::search(catalog.entries(), QString());
  QVERIFY(!empty.ok());
  QCOMPARE(empty.error, SearchErrorCode::EmptyQuery);

  const auto blank = LauncherSearchRanker::search(catalog.entries(),
                                                  QStringLiteral("   \t "));
  QVERIFY(!blank.ok());
  QCOMPARE(blank.error, SearchErrorCode::EmptyQuery);
}

void LauncherSearchRankerTest::oversizeQueryIsRejected()
{
  const auto catalog = ApplicationCatalog::build(catalogCorpus());

  const QString oversize = QStringLiteral("a").repeated(Bounds::maxQueryLength + 1);
  const auto outcome = LauncherSearchRanker::search(catalog.entries(), oversize);
  QVERIFY(!outcome.ok());
  QCOMPARE(outcome.error, SearchErrorCode::QueryTooLong);
}

void LauncherSearchRankerTest::noMatchYieldsEmptyOkOutcome()
{
  const auto catalog = ApplicationCatalog::build(catalogCorpus());

  const auto outcome =
      LauncherSearchRanker::search(catalog.entries(), QStringLiteral("zzzz"));
  QVERIFY(outcome.ok());
  QVERIFY(outcome.results.isEmpty());
}

void LauncherSearchRankerTest::matchKindEnumOrderIsTheScoreOrder()
{
  QVERIFY(SearchMatchKind::NamePrefix < SearchMatchKind::NameWordStart);
  QVERIFY(SearchMatchKind::NameWordStart < SearchMatchKind::NameSubstring);
  QVERIFY(SearchMatchKind::NameSubstring < SearchMatchKind::KeywordPrefix);
  QVERIFY(SearchMatchKind::KeywordPrefix < SearchMatchKind::KeywordSubstring);
  QVERIFY(SearchMatchKind::KeywordSubstring < SearchMatchKind::GenericNameSubstring);
  QVERIFY(SearchMatchKind::GenericNameSubstring < SearchMatchKind::CommentSubstring);
}

QTEST_GUILESS_MAIN(LauncherSearchRankerTest)
#include "tst_launcher_search_ranker.moc"
