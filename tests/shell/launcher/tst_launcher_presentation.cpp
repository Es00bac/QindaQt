// SPDX-License-Identifier: GPL-3.0-or-later
#include "launcher_test_support.h"

#include "qindaqt/shell_launcher/launcher_bounds.h"
#include "qindaqt/shell_launcher/launcher_presentation.h"

#include <QTest>

using namespace QindaQt::ShellLauncher;
using namespace QindaQt::ShellLauncher::TestSupport;

namespace {

PinnedApplications pinnedWith(const QVector<QString> &ids)
{
  PinnedApplications pinned;
  for (const QString &id : ids)
    pinned.pin(id);
  return pinned;
}

RecentApplications recentWith(const QVector<QString> &ids)
{
  RecentApplications recent;
  for (const QString &id : ids)
    recent.record(id);
  return recent;
}

QVector<QString> sectionEntryIds(const LauncherPresentation &presentation,
                                 SectionKind kind)
{
  QVector<QString> ids;
  for (const PresentationSection &section : presentation.sections) {
    if (section.kind != kind)
      continue;
    for (const PresentationItem &item : section.items)
      ids.append(item.entryId);
  }
  return ids;
}

} // namespace

class LauncherPresentationTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void absentCatalogMeansLoading();
  void readyCatalogProducesSectionsInFixedOrder();
  void emptyAndDegradedStatusesFollowTheCatalog();
  void searchCollapsesToOneResultsSection();
  void emptySearchStillPublishesResultsSection();
  void pinnedAndRecentDropUnknownIdsInModelError();
  void invalidQueryFallsBackToBrowsePresentation();
  void itemsCarryAccessibleValues();
  void launchIntentIsConfinedAndUsesActionIconFallback();
  void flatItemIndexingMatchesFocusOrder();
};

void LauncherPresentationTest::absentCatalogMeansLoading()
{
  const auto presentation = LauncherPresentationModel::build(
      std::nullopt, pinnedWith({}), recentWith({}));
  QCOMPARE(presentation.status, LauncherStatus::Loading);
  QVERIFY(presentation.sections.isEmpty());
  QCOMPARE(presentation.itemCount(), 0);
}

void LauncherPresentationTest::readyCatalogProducesSectionsInFixedOrder()
{
  const auto catalog = ApplicationCatalog::build(catalogCorpus());
  const auto presentation = LauncherPresentationModel::build(
      catalog, pinnedWith({ QStringLiteral("org.qinda.music") }),
      recentWith({ QStringLiteral("org.qinda.viewer") }));

  QCOMPARE(presentation.status, LauncherStatus::Ready);

  QVector<SectionKind> kinds;
  QVector<SectionLabel> labels;
  QVector<LauncherCategory> categories;
  for (const PresentationSection &section : presentation.sections) {
    kinds.append(section.kind);
    labels.append(section.label);
    categories.append(section.category);
  }
  QCOMPARE(kinds,
           (QVector<SectionKind> { SectionKind::Pinned, SectionKind::Recent,
                                   SectionKind::Categories, SectionKind::Categories,
                                   SectionKind::Categories, SectionKind::Categories,
                                   SectionKind::Categories }));
  QCOMPARE(labels,
           (QVector<SectionLabel> { SectionLabel::Pinned, SectionLabel::Recent,
                                    SectionLabel::Category, SectionLabel::Category,
                                    SectionLabel::Category, SectionLabel::Category,
                                    SectionLabel::Category }));
  QCOMPARE(categories,
           (QVector<LauncherCategory> { LauncherCategory::Other,
                                        LauncherCategory::Other,
                                        LauncherCategory::Graphics,
                                        LauncherCategory::AudioVideo,
                                        LauncherCategory::Network,
                                        LauncherCategory::Office,
                                        LauncherCategory::System }));
  QCOMPARE(sectionEntryIds(presentation, SectionKind::Pinned),
           QVector<QString> { QStringLiteral("org.qinda.music") });
  QCOMPARE(sectionEntryIds(presentation, SectionKind::Recent),
           QVector<QString> { QStringLiteral("org.qinda.viewer") });
  // Five visible entries; the pinned and recent sections repeat identities,
  // so the flat item count exceeds the catalog size.
  QCOMPARE(presentation.itemCount(), 7);
}

void LauncherPresentationTest::emptySearchStillPublishesResultsSection()
{
  const auto catalog = ApplicationCatalog::build(catalogCorpus());
  const auto presentation = LauncherPresentationModel::build(
      catalog, pinnedWith({}), recentWith({}), QStringLiteral("no-match-value"));

  QCOMPARE(presentation.status, LauncherStatus::Ready);
  QCOMPARE(presentation.sections.size(), 1);
  QCOMPARE(presentation.sections.first().kind, SectionKind::SearchResults);
  QCOMPARE(presentation.sections.first().label, SectionLabel::SearchResults);
  QVERIFY(presentation.sections.first().items.isEmpty());
}

void LauncherPresentationTest::emptyAndDegradedStatusesFollowTheCatalog()
{
  const auto emptyCatalog = ApplicationCatalog::build({});
  const auto emptyPresentation = LauncherPresentationModel::build(
      emptyCatalog, pinnedWith({}), recentWith({}));
  QCOMPARE(emptyPresentation.status, LauncherStatus::Empty);
  QVERIFY(emptyPresentation.sections.isEmpty());

  EntryTemplate broken;
  broken.name = QString();
  const auto degradedCatalog = ApplicationCatalog::build(
      { document(QStringLiteral("app.broken"), broken),
        document(QStringLiteral("app.good"), EntryTemplate {}) });
  const auto degradedPresentation = LauncherPresentationModel::build(
      degradedCatalog, pinnedWith({}), recentWith({}));
  QCOMPARE(degradedPresentation.status, LauncherStatus::Degraded);
  // Degraded still offers the entries that did validate.
  QCOMPARE(degradedPresentation.itemCount(), 1);
  QCOMPARE(degradedPresentation.sections.first().kind, SectionKind::Categories);
}

void LauncherPresentationTest::searchCollapsesToOneResultsSection()
{
  const auto catalog = ApplicationCatalog::build(catalogCorpus());
  const auto presentation = LauncherPresentationModel::build(
      catalog, pinnedWith({ QStringLiteral("org.qinda.editor") }),
      recentWith({}), QStringLiteral("qinda"));

  QCOMPARE(presentation.status, LauncherStatus::Ready);
  QCOMPARE(presentation.sections.size(), 1);
  QCOMPARE(presentation.sections.first().kind, SectionKind::SearchResults);
  QCOMPARE(sectionEntryIds(presentation, SectionKind::SearchResults),
           (QVector<QString> { QStringLiteral("org.qinda.editor"),
                               QStringLiteral("org.qinda.terminal") }));
  // The pinned flag still travels into search results.
  QVERIFY(presentation.sections.first().items.first().pinned);
  QVERIFY(!presentation.sections.first().items.last().pinned);
}

void LauncherPresentationTest::pinnedAndRecentDropUnknownIdsInModelError()
{
  const auto catalog = ApplicationCatalog::build(catalogCorpus());
  const auto presentation = LauncherPresentationModel::build(
      catalog, pinnedWith({ QStringLiteral("app.gone"),
                            QStringLiteral("org.qinda.music") }),
      recentWith({ QStringLiteral("app.hidden"),
                   QStringLiteral("org.qinda.viewer") }));

  // Stale pinned/recent identities from an older session degrade quietly:
  // they vanish from the surface and never fabricate items.
  QCOMPARE(sectionEntryIds(presentation, SectionKind::Pinned),
           QVector<QString> { QStringLiteral("org.qinda.music") });
  QCOMPARE(sectionEntryIds(presentation, SectionKind::Recent),
           QVector<QString> { QStringLiteral("org.qinda.viewer") });
  QCOMPARE(presentation.status, LauncherStatus::Ready);
}

void LauncherPresentationTest::invalidQueryFallsBackToBrowsePresentation()
{
  const auto catalog = ApplicationCatalog::build(catalogCorpus());

  const auto blank = LauncherPresentationModel::build(
      catalog, pinnedWith({}), recentWith({}), QStringLiteral("   "));
  QCOMPARE(blank.status, LauncherStatus::Ready);
  QCOMPARE(blank.sections.first().kind, SectionKind::Categories);

  const QString oversize = QStringLiteral("a").repeated(Bounds::maxQueryLength + 1);
  const auto oversizePresentation = LauncherPresentationModel::build(
      catalog, pinnedWith({}), recentWith({}), oversize);
  QCOMPARE(oversizePresentation.status, LauncherStatus::Ready);
  QVERIFY(!oversizePresentation.sections.isEmpty());
  QCOMPARE(oversizePresentation.sections.first().kind, SectionKind::Categories);
}

void LauncherPresentationTest::itemsCarryAccessibleValues()
{
  const auto catalog = ApplicationCatalog::build(catalogCorpus());
  const auto presentation = LauncherPresentationModel::build(
      catalog, pinnedWith({}), recentWith({}));

  bool foundEditor = false;
  for (const PresentationSection &section : presentation.sections) {
    for (const PresentationItem &item : section.items) {
      QVERIFY(!item.displayText.isEmpty());
      if (item.entryId == QStringLiteral("org.qinda.editor")) {
        foundEditor = true;
        QCOMPARE(item.displayText, QStringLiteral("Qinda Editor"));
        QCOMPARE(item.iconName, QStringLiteral("accessories-text-editor"));
        QCOMPARE(item.accessibleDescription,
                 QStringLiteral("Write and edit documents"));
        QCOMPARE(item.accessibleRole, AccessibleRole::ListItem);
      }
    }
  }
  QVERIFY(foundEditor);

  EntryTemplate fallback;
  fallback.name = QStringLiteral("Fallback Name");
  fallback.comment.clear();
  fallback.genericName.clear();
  const auto fallbackPresentation = LauncherPresentationModel::build(
      ApplicationCatalog::build(
          { document(QStringLiteral("app.fallback"), fallback) }),
      pinnedWith({}), recentWith({}));
  QCOMPARE(fallbackPresentation.sections.first().items.first().accessibleDescription,
           QStringLiteral("Fallback Name"));
}

void LauncherPresentationTest::launchIntentIsConfinedAndUsesActionIconFallback()
{
  const auto catalog = ApplicationCatalog::build(catalogCorpus());

  const auto intent = catalog.makeLaunchIntent(
      QStringLiteral("org.qinda.editor"), QStringLiteral("new-window"));
  QVERIFY(intent.ok());
  QCOMPARE(*intent.intent,
           (LaunchIntent { QStringLiteral("org.qinda.editor"),
                           QStringLiteral("new-window"),
                           QStringLiteral("New Window"),
                           QStringLiteral("accessories-text-editor") }));
}

void LauncherPresentationTest::flatItemIndexingMatchesFocusOrder()
{
  const auto catalog = ApplicationCatalog::build(catalogCorpus());
  const auto presentation = LauncherPresentationModel::build(
      catalog, pinnedWith({ QStringLiteral("org.qinda.music") }),
      recentWith({ QStringLiteral("org.qinda.viewer") }));

  QCOMPARE(presentation.itemCount(), 7);
  // Focus order is exactly section order then item order.
  QCOMPARE(presentation.itemAt(0)->entryId, QStringLiteral("org.qinda.music"));
  QCOMPARE(presentation.itemAt(1)->entryId, QStringLiteral("org.qinda.viewer"));
  QVERIFY(!presentation.itemAt(7).has_value());
  QVERIFY(!presentation.itemAt(-1).has_value());
}

QTEST_GUILESS_MAIN(LauncherPresentationTest)
#include "tst_launcher_presentation.moc"
