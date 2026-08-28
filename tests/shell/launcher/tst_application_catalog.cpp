// SPDX-License-Identifier: GPL-3.0-or-later
#include "launcher_test_support.h"

#include "qindaqt/shell_launcher/application_catalog.h"
#include "qindaqt/shell_launcher/launcher_bounds.h"

#include <QTest>

using namespace QindaQt::ShellLauncher;
using namespace QindaQt::ShellLauncher::TestSupport;

namespace {

QString paddedName(int length)
{
  return QString(length, QLatin1Char('A'));
}

} // namespace

class ApplicationCatalogTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void buildsDeterministicDisplayOrder();
  void visibleEntriesExcludeHiddenDocuments();
  void firstDocumentWinsForDuplicateIds();
  void firstDocumentClaimsIdBeforeVisibilityOrParsing();
  void invalidDocumentsDegradeWithoutKillingTheCatalog();
  void entryLookupAndLaunchIntentResolution();
  void launchIntentRejectsUnknownEntryAndAction();
  void launchIntentPrefersActionDisplayValues();
  void emptyCorpusIsEmptyNotDegraded();
  void entryCeilingStopsParsingWithDiagnostic();
  void diagnosticIdentityIsBounded();
};

void ApplicationCatalogTest::buildsDeterministicDisplayOrder()
{
  const auto catalog = ApplicationCatalog::build(catalogCorpus());
  QVERIFY(catalog.diagnostics().isEmpty());

  QVector<QString> names;
  for (const ApplicationEntry &entry : catalog.entries())
    names.append(entry.name);
  QCOMPARE(names, (QVector<QString> { QStringLiteral("Image Viewer"),
                                      QStringLiteral("Music Player"),
                                      QStringLiteral("Qinda Editor"),
                                      QStringLiteral("Qinda Terminal"),
                                      QStringLiteral("Web Browser") }));
}

void ApplicationCatalogTest::visibleEntriesExcludeHiddenDocuments()
{
  EntryTemplate hiddenEntry;
  hiddenEntry.name = QStringLiteral("Hidden Tool");
  hiddenEntry.noDisplayLine = QStringLiteral("NoDisplay=true");

  EntryTemplate retiredEntry;
  retiredEntry.name = QStringLiteral("Retired Tool");
  retiredEntry.hiddenLine = QStringLiteral("Hidden=true");

  const auto catalog = ApplicationCatalog::build(
      { document(QStringLiteral("app.hidden"), hiddenEntry),
        document(QStringLiteral("app.retired"), retiredEntry),
        document(QStringLiteral("app.visible"),
                 EntryTemplate {}) });
  QVERIFY(catalog.diagnostics().isEmpty());
  QCOMPARE(catalog.entries().size(), 1);
  QCOMPARE(catalog.entries().first().id, QStringLiteral("app.visible"));
}

void ApplicationCatalogTest::firstDocumentWinsForDuplicateIds()
{
  EntryTemplate first;
  first.name = QStringLiteral("First Winner");
  EntryTemplate second;
  second.name = QStringLiteral("Second Loser");

  const auto catalog = ApplicationCatalog::build(
      { document(QStringLiteral("app.dupe"), first),
        document(QStringLiteral("app.dupe"), second) });
  QCOMPARE(catalog.entries().size(), 1);
  QCOMPARE(catalog.entries().first().name, QStringLiteral("First Winner"));
  QCOMPARE(catalog.diagnostics().size(), 1);
  QCOMPARE(catalog.diagnostics().first().kind, DiagnosticKind::DuplicateEntryId);
  QCOMPARE(catalog.diagnostics().first().sourceId, QStringLiteral("app.dupe"));
}

void ApplicationCatalogTest::firstDocumentClaimsIdBeforeVisibilityOrParsing()
{
  EntryTemplate hidden;
  hidden.hiddenLine = QStringLiteral("Hidden=true");
  EntryTemplate noDisplay;
  noDisplay.noDisplayLine = QStringLiteral("NoDisplay=true");
  EntryTemplate invalid;
  invalid.name.clear();
  EntryTemplate visible;
  visible.name = QStringLiteral("Visible Winner");

  const auto catalog = ApplicationCatalog::build(
      { document(QStringLiteral("app.hidden-first"), hidden),
        document(QStringLiteral("app.hidden-first"), visible),
        document(QStringLiteral("app.nodisplay-first"), noDisplay),
        document(QStringLiteral("app.nodisplay-first"), visible),
        document(QStringLiteral("app.invalid-first"), invalid),
        document(QStringLiteral("app.invalid-first"), visible),
        document(QStringLiteral("app.visible-first"), visible),
        document(QStringLiteral("app.visible-first"), hidden) });

  QCOMPARE(catalog.entries().size(), 1);
  QCOMPARE(catalog.entries().first().id, QStringLiteral("app.visible-first"));
  QCOMPARE(catalog.diagnostics().size(), 5);
  QCOMPARE(catalog.diagnostics().at(0).kind, DiagnosticKind::DuplicateEntryId);
  QCOMPARE(catalog.diagnostics().at(1).kind, DiagnosticKind::DuplicateEntryId);
  QCOMPARE(catalog.diagnostics().at(2).kind, DiagnosticKind::InvalidDocument);
  QCOMPARE(catalog.diagnostics().at(3).kind, DiagnosticKind::DuplicateEntryId);
  QCOMPARE(catalog.diagnostics().at(4).kind, DiagnosticKind::DuplicateEntryId);
}

void ApplicationCatalogTest::invalidDocumentsDegradeWithoutKillingTheCatalog()
{
  EntryTemplate broken;
  broken.name = QString();

  const auto catalog = ApplicationCatalog::build(
      { document(QStringLiteral("app.broken"), broken),
        document(QStringLiteral("app.good"), EntryTemplate {}) });
  QCOMPARE(catalog.entries().size(), 1);
  QCOMPARE(catalog.entries().first().id, QStringLiteral("app.good"));
  QCOMPARE(catalog.diagnostics().size(), 1);
  QCOMPARE(catalog.diagnostics().first().kind, DiagnosticKind::InvalidDocument);
}

void ApplicationCatalogTest::entryLookupAndLaunchIntentResolution()
{
  const auto catalog = ApplicationCatalog::build(catalogCorpus());

  const auto editor = catalog.entry(QStringLiteral("org.qinda.editor"));
  QVERIFY(editor.has_value());
  QVERIFY(!catalog.entry(QStringLiteral("app.missing")).has_value());

  const auto intent = catalog.makeLaunchIntent(QStringLiteral("org.qinda.editor"));
  QVERIFY2(intent.ok(), qPrintable(QStringLiteral("intent expected")));
  QCOMPARE(intent.intent->entryId, QStringLiteral("org.qinda.editor"));
  QVERIFY(intent.intent->actionId.isEmpty());
  QCOMPARE(intent.intent->displayName, QStringLiteral("Qinda Editor"));
  QCOMPARE(intent.intent->iconName, QStringLiteral("accessories-text-editor"));
}

void ApplicationCatalogTest::launchIntentRejectsUnknownEntryAndAction()
{
  const auto catalog = ApplicationCatalog::build(catalogCorpus());

  const auto unknownEntry =
      catalog.makeLaunchIntent(QStringLiteral("app.missing"));
  QCOMPARE(unknownEntry.error, LaunchIntentError::UnknownEntry);
  QVERIFY(!unknownEntry.ok());

  const auto unknownAction = catalog.makeLaunchIntent(
      QStringLiteral("org.qinda.editor"), QStringLiteral("no-such-action"));
  QCOMPARE(unknownAction.error, LaunchIntentError::UnknownAction);
  QVERIFY(!unknownAction.ok());
}

void ApplicationCatalogTest::launchIntentPrefersActionDisplayValues()
{
  const auto catalog = ApplicationCatalog::build(catalogCorpus());

  const auto intent = catalog.makeLaunchIntent(
      QStringLiteral("org.qinda.editor"), QStringLiteral("new-window"));
  QVERIFY2(intent.ok(), qPrintable(QStringLiteral("action intent expected")));
  QCOMPARE(intent.intent->actionId, QStringLiteral("new-window"));
  QCOMPARE(intent.intent->displayName, QStringLiteral("New Window"));
  QCOMPARE(intent.intent->iconName, QStringLiteral("accessories-text-editor"));
}

void ApplicationCatalogTest::diagnosticIdentityIsBounded()
{
  const QString hostileId(Bounds::maxEntryIdLength + 4096, QLatin1Char('x'));
  const auto catalog = ApplicationCatalog::build(
      { document(hostileId, EntryTemplate {}) });
  QCOMPARE(catalog.entries().size(), 0);
  QCOMPARE(catalog.diagnostics().size(), 1);
  QCOMPARE(catalog.diagnostics().first().kind, DiagnosticKind::InvalidDocument);
  QCOMPARE(catalog.diagnostics().first().sourceId.size(),
           Bounds::maxEntryIdLength);
}

void ApplicationCatalogTest::emptyCorpusIsEmptyNotDegraded()
{
  const auto catalog = ApplicationCatalog::build({});
  QVERIFY(catalog.entries().isEmpty());
  QVERIFY(catalog.diagnostics().isEmpty());
}

void ApplicationCatalogTest::entryCeilingStopsParsingWithDiagnostic()
{
  QVector<SourceDocument> documents;
  const int overTheCeiling = Bounds::maxVisibleEntries + 4;
  EntryTemplate entry;
  for (int index = 0; index < overTheCeiling; ++index) {
    entry.name = paddedName(1) + QStringLiteral("%1").arg(index, 6, 10, QLatin1Char('0'));
    documents.append(document(QStringLiteral("app.%1").arg(index, 6, 10, QLatin1Char('0')),
                              entry));
  }

  const auto catalog = ApplicationCatalog::build(documents);
  QCOMPARE(catalog.entries().size(), Bounds::maxVisibleEntries);
  QCOMPARE(catalog.diagnostics().size(), 1);
  QCOMPARE(catalog.diagnostics().first().kind, DiagnosticKind::EntryLimitReached);
  QVERIFY(!catalog.diagnosticsTruncated());
}

QTEST_GUILESS_MAIN(ApplicationCatalogTest)
#include "tst_application_catalog.moc"
