// SPDX-License-Identifier: GPL-3.0-or-later
// Registry and controller side of Settings search (ADR-0257): every route
// carries search keywords, only routes whose page honours a destination list
// one, the ids match the page, and the projection and deep-link re-delivery
// that the QML palette depends on hold without QML.
#include "src/apps/settings_center/settings_navigation_controller.h"
#include "src/apps/settings_center/settings_route_registry.h"

#include <QFile>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTest>

using namespace QindaQt::Apps::SettingsCenter;

namespace {

SettingsRoute plainRoute(const QString &id) {
  return SettingsRoute{
      .id = id,
      .component = SettingsRouteComponent::Notifications,
      .title = QStringLiteral("Plain"),
      .description = QString(),
      .iconName = QString(),
      .category = QStringLiteral("General"),
      .available = true,
      .unavailableReason = QString(),
  };
}

} // namespace

class SettingsRouteSearchTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void everyBuiltInRouteHasKeywords();
  void searchMetadataLeavesOrderAndDigitRoutesUnchanged();
  void onlyInputDeclaresDestinations();
  void inputDestinationsMatchTheInputPage();
  void hostileSearchMetadataIsRejected();
  void controllerProjectsKeywordsAndDestinations();
  void repeatedDestinationIsRedeliveredToTheOpenPage();
};

void SettingsRouteSearchTest::everyBuiltInRouteHasKeywords() {
  const SettingsRouteRegistry registry = SettingsRouteRegistry::createDefault();
  QCOMPARE(registry.count(), 21);
  for (const SettingsRoute &route : registry.routes()) {
    QVERIFY2(route.isValid(), qPrintable(route.id));
    QVERIFY2(!route.keywords.isEmpty(), qPrintable(route.id));
    for (const QString &keyword : route.keywords) {
      QCOMPARE(keyword, keyword.trimmed());
    }
  }
  // The examples the plan promises must reach their page by keyword alone.
  QVERIFY(registry.route(QStringLiteral("network"))
              ->keywords.contains(QStringLiteral("wifi")));
  QVERIFY(registry.route(QStringLiteral("power"))
              ->keywords.contains(QStringLiteral("battery")));
  // W15: Customize is the layout preset page now (ADR-0267).
  QVERIFY(registry.route(QStringLiteral("customize"))
              ->keywords.contains(QStringLiteral("layout presets")));
}

void SettingsRouteSearchTest::searchMetadataLeavesOrderAndDigitRoutesUnchanged() {
  // AGENT-GUARD: attaching search metadata after registration must not move
  // a route; the first ten keep Ctrl+1..Ctrl+0 (ADR-0128). This list and
  // tst_settings_route_registry.cpp change together when a route is added.
  const QStringList expected{
      QStringLiteral("notifications"), QStringLiteral("appearance"),
      QStringLiteral("display"),       QStringLiteral("network"),
      QStringLiteral("customize"),     QStringLiteral("audio"),
      QStringLiteral("bluetooth"),     QStringLiteral("power"),
      QStringLiteral("clipboard"),     QStringLiteral("color"),
      QStringLiteral("accessibility"), QStringLiteral("input"),
      QStringLiteral("streaming"),     QStringLiteral("datetime"),
      QStringLiteral("windows"),       QStringLiteral("default-apps"),
      QStringLiteral("about-computer"), QStringLiteral("startup"),
      QStringLiteral("screensaver"),   QStringLiteral("login-screen"),
      QStringLiteral("voice"),
  };
  const SettingsRouteRegistry registry = SettingsRouteRegistry::createDefault();
  QStringList actual;
  for (const SettingsRoute &route : registry.routes()) {
    actual.append(route.id);
  }
  QCOMPARE(actual, expected);

  SettingsNavigationController navigation(registry);
  const QVariantList routes = navigation.routesList();
  QCOMPARE(routes.size(), expected.size());
  for (qsizetype index = 0; index < routes.size(); ++index) {
    QCOMPARE(routes.at(index).toMap().value(QStringLiteral("id")).toString(),
             expected.at(index));
  }
  // Ctrl+0 is the tenth route, Color; Accessibility is the first without one.
  QVERIFY(navigation.selectIndex(9));
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("color"));
}

void SettingsRouteSearchTest::onlyInputDeclaresDestinations() {
  const SettingsRouteRegistry registry = SettingsRouteRegistry::createDefault();
  for (const SettingsRoute &route : registry.routes()) {
    if (route.component == SettingsRouteComponent::Input) {
      QCOMPARE(route.destinations.size(), 5);
    } else {
      QVERIFY2(route.destinations.isEmpty(), qPrintable(route.id));
    }
  }
  const auto input = registry.route(QStringLiteral("input"));
  QVERIFY(input.has_value());
  QStringList ids;
  for (const SettingsRouteDestination &destination : input->destinations) {
    QVERIFY2(destination.isValid(), qPrintable(destination.id));
    QVERIFY2(!destination.keywords.isEmpty(), qPrintable(destination.id));
    ids.append(destination.id);
  }
  QCOMPARE(ids, (QStringList{QStringLiteral("pointers"), QStringLiteral("tablet"),
                             QStringLiteral("keyboard"), QStringLiteral("shortcuts"),
                             QStringLiteral("touch")}));
}

void SettingsRouteSearchTest::inputDestinationsMatchTheInputPage() {
  // AGENT-CONTRACT: the Input page is the authority on its destinations
  // (settings_route_search_metadata.cpp). Read its source list so a renamed
  // or added sub-page fails here instead of opening the default page.
  QFile page(QStringLiteral(QINDAQT_SOURCE_DIR
                            "/src/apps/settings/input/qml/InputPage.qml"));
  QVERIFY2(page.open(QIODevice::ReadOnly | QIODevice::Text),
           qPrintable(page.errorString()));
  const QString source = QString::fromUtf8(page.readAll());
  static const QRegularExpression entry(
      QString::fromUtf8(
          R"re(\{\s*id:\s*"([a-z0-9_-]+)",\s*title:\s*qsTr\("([^"]+)"\))re"));
  QList<QPair<QString, QString>> pageDestinations;
  for (auto match = entry.globalMatch(source); match.hasNext();) {
    const auto found = match.next();
    pageDestinations.append({found.captured(1), found.captured(2)});
  }
  QCOMPARE(pageDestinations.size(), 5);

  const auto input =
      SettingsRouteRegistry::createDefault().route(QStringLiteral("input"));
  QVERIFY(input.has_value());
  QCOMPARE(input->destinations.size(), pageDestinations.size());
  for (qsizetype index = 0; index < pageDestinations.size(); ++index) {
    QCOMPARE(input->destinations.at(index).id, pageDestinations.at(index).first);
    QCOMPARE(input->destinations.at(index).title,
             pageDestinations.at(index).second);
  }
}

void SettingsRouteSearchTest::hostileSearchMetadataIsRejected() {
  SettingsRoute route = plainRoute(QStringLiteral("plain"));
  QVERIFY(route.isValid());

  route.keywords = {QStringLiteral("   ")};
  QVERIFY(!route.isValid());
  route.keywords = {QStringLiteral("a") + QChar(QChar::Null)};
  QVERIFY(!route.isValid());
  route.keywords = {QString(MaximumSearchKeywordLength + 1, QLatin1Char('k'))};
  QVERIFY(!route.isValid());
  route.keywords = QStringList(MaximumSearchKeywordCount + 1, QStringLiteral("k"));
  QVERIFY(!route.isValid());
  route.keywords = {QStringLiteral("fine")};
  QVERIFY(route.isValid());

  const SettingsRouteDestination good{.id = QStringLiteral("one"),
                                      .title = QStringLiteral("One")};
  route.destinations = {good};
  QVERIFY(route.isValid());
  route.destinations = {good, good};
  QVERIFY2(!route.isValid(), "duplicate destination ids");
  route.destinations = {{.id = QStringLiteral("../x"), .title = QStringLiteral("X")}};
  QVERIFY2(!route.isValid(), "destination id outside route-id syntax");
  route.destinations = {{.id = QStringLiteral("x"), .title = QStringLiteral(" ")}};
  QVERIFY2(!route.isValid(), "blank destination title");
  route.destinations = {{.id = QStringLiteral("x"),
                         .title = QStringLiteral("X"),
                         .keywords = {QString()}}};
  QVERIFY2(!route.isValid(), "blank destination keyword");
  route.destinations = QList<SettingsRouteDestination>(
      SettingsRoute::MaximumDestinationCount + 1, good);
  QVERIFY(!route.isValid());

  SettingsRouteRegistry registry;
  QString error;
  QVERIFY(!registry.registerRoute(route, &error));
  QVERIFY(!error.isEmpty());
  QCOMPARE(registry.count(), 0);
}

void SettingsRouteSearchTest::controllerProjectsKeywordsAndDestinations() {
  SettingsNavigationController navigation(SettingsRouteRegistry::createDefault());
  const QVariantMap network = navigation.routeAt(3);
  QCOMPARE(network.value(QStringLiteral("id")).toString(),
           QStringLiteral("network"));
  QVERIFY(network.value(QStringLiteral("keywords"))
              .toStringList()
              .contains(QStringLiteral("wifi")));
  QVERIFY(network.value(QStringLiteral("destinations")).toList().isEmpty());

  const QVariantMap input = navigation.routeAt(11);
  QCOMPARE(input.value(QStringLiteral("id")).toString(), QStringLiteral("input"));
  const QVariantList destinations =
      input.value(QStringLiteral("destinations")).toList();
  QCOMPARE(destinations.size(), 5);
  const QVariantMap shortcuts = destinations.at(3).toMap();
  QCOMPARE(shortcuts.value(QStringLiteral("id")).toString(),
           QStringLiteral("shortcuts"));
  QCOMPARE(shortcuts.value(QStringLiteral("title")).toString(),
           QStringLiteral("Shortcuts"));
  QVERIFY(!shortcuts.value(QStringLiteral("keywords")).toStringList().isEmpty());
}

void SettingsRouteSearchTest::repeatedDestinationIsRedeliveredToTheOpenPage() {
  SettingsNavigationController navigation(SettingsRouteRegistry::createDefault(),
                                          QStringLiteral("network"));
  QSignalSpy linkSpy(&navigation,
                     &SettingsNavigationController::requestedDeepLinkChanged);
  QSignalSpy routeSpy(&navigation,
                      &SettingsNavigationController::activeRouteIdChanged);

  QVERIFY(navigation.selectRouteDestination(QStringLiteral("input"),
                                            QStringLiteral("shortcuts")));
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("input"));
  QCOMPARE(navigation.requestedDestination(), QStringLiteral("shortcuts"));
  QCOMPARE(linkSpy.count(), 1);
  QCOMPARE(routeSpy.count(), 1);

  // Same request while Input is open: cleared, then set again, so the open
  // page observes it even though the value ends where it started.
  QVERIFY(navigation.selectRouteDestination(QStringLiteral("input"),
                                            QStringLiteral("shortcuts")));
  QCOMPARE(linkSpy.count(), 3);
  QCOMPARE(routeSpy.count(), 1);
  QCOMPARE(navigation.requestedDestination(), QStringLiteral("shortcuts"));

  // A different destination is an ordinary single change.
  QVERIFY(navigation.selectRouteDestination(QStringLiteral("input"),
                                            QStringLiteral("keyboard")));
  QCOMPARE(linkSpy.count(), 4);

  // Arriving from another route with the same link needs no re-delivery: the
  // page is created and reads the link at construction.
  QVERIFY(navigation.selectRoute(QStringLiteral("network")));
  QVERIFY(navigation.selectRouteDestination(QStringLiteral("input"),
                                            QStringLiteral("keyboard")));
  QCOMPARE(linkSpy.count(), 4);
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("input"));

  // Unknown routes still change nothing.
  QVERIFY(!navigation.selectRouteDestination(QStringLiteral("missing"),
                                             QStringLiteral("keyboard")));
  QCOMPARE(linkSpy.count(), 4);
  QCOMPARE(navigation.requestedDestination(), QStringLiteral("keyboard"));
}

QTEST_GUILESS_MAIN(SettingsRouteSearchTest)
#include "tst_settings_route_search.moc"
