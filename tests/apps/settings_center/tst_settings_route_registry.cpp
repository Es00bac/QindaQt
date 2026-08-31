// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/apps/settings_center/settings_route.h"
#include "src/apps/settings_center/settings_route_registry.h"

#include <QTest>

using namespace QindaQt::Apps::SettingsCenter;

class SettingsRouteRegistryTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void testRouteIdValidation();
  void testRouteValidationConstraints();
  void testRegistryRegistrationAndLookup();
  void testRegistryRejectsDuplicates();
  void testRegistryCapacityEnforcement();
  void testBuiltInRoutesIntegrity();
  void testRouteVariantMapConversion();
};

void SettingsRouteRegistryTest::testRouteIdValidation() {
  // Valid IDs
  QVERIFY(isValidRouteId(QStringLiteral("notifications")));
  QVERIFY(isValidRouteId(QStringLiteral("appearance")));
  QVERIFY(isValidRouteId(QStringLiteral("display-settings")));
  QVERIFY(isValidRouteId(QStringLiteral("power_and_battery")));
  QVERIFY(isValidRouteId(QStringLiteral("a123-456_789")));

  // Invalid IDs
  QVERIFY(!isValidRouteId(QString()));                           // empty
  QVERIFY(!isValidRouteId(QStringLiteral("has space")));         // spaces
  QVERIFY(!isValidRouteId(QStringLiteral("notifications/sub"))); // slash
  QVERIFY(!isValidRouteId(QStringLiteral("route.dot")));         // dot
  QVERIFY(!isValidRouteId(QStringLiteral("unicode-ñ-route")));   // non-ascii
  QVERIFY(!isValidRouteId(
      QStringLiteral("Appearance"))); // uppercase is not canonical
  QVERIFY(!isValidRouteId(QStringLiteral("-leading")));
  QVERIFY(!isValidRouteId(QStringLiteral("_leading")));
  QVERIFY(!isValidRouteId(QString(65, QLatin1Char('a')))); // > 64 chars
  QVERIFY(isValidRouteId(QString(64, QLatin1Char('a'))));  // exactly 64 chars

  // Null bytes embedded
  QString nullEmbedded = QStringLiteral("route");
  nullEmbedded.append(QChar(0));
  nullEmbedded.append(QStringLiteral("tail"));
  QVERIFY(!isValidRouteId(nullEmbedded));
}

void SettingsRouteRegistryTest::testRouteValidationConstraints() {
  SettingsRoute validRoute{
      .id = QStringLiteral("valid-route"),
      .component = SettingsRouteComponent::Appearance,
      .title = QStringLiteral("Valid Title"),
      .description = QStringLiteral("A valid route description"),
      .iconName = QStringLiteral("preferences-other"),
      .category = QStringLiteral("System"),
      .available = true,
      .unavailableReason = QString(),
  };
  QVERIFY(validRoute.isValid());

  // Empty title rejected
  SettingsRoute emptyTitle = validRoute;
  emptyTitle.title = QString();
  QVERIFY(!emptyTitle.isValid());

  SettingsRoute whitespaceTitle = validRoute;
  whitespaceTitle.title = QStringLiteral("   \t  ");
  QVERIFY(!whitespaceTitle.isValid());

  // Overly long title rejected (> 128 chars)
  SettingsRoute longTitle = validRoute;
  longTitle.title = QString(129, QLatin1Char('T'));
  QVERIFY(!longTitle.isValid());

  // Overly long description rejected (> 256 chars)
  SettingsRoute longDesc = validRoute;
  longDesc.description = QString(257, QLatin1Char('D'));
  QVERIFY(!longDesc.isValid());

  // Overly long category rejected (> 64 chars)
  SettingsRoute longCat = validRoute;
  longCat.category = QString(65, QLatin1Char('C'));
  QVERIFY(!longCat.isValid());

  SettingsRoute invalidComponent = validRoute;
  invalidComponent.component = static_cast<SettingsRouteComponent>(200);
  QVERIFY(!invalidComponent.isValid());

  SettingsRoute unavailableWithoutReason = validRoute;
  unavailableWithoutReason.available = false;
  QVERIFY(!unavailableWithoutReason.isValid());

  SettingsRoute availableWithReason = validRoute;
  availableWithReason.unavailableReason =
      QStringLiteral("hidden contradiction");
  QVERIFY(!availableWithReason.isValid());

  SettingsRoute unavailable = validRoute;
  unavailable.available = false;
  unavailable.unavailableReason = QStringLiteral("Provider unavailable");
  QVERIFY(unavailable.isValid());

  SettingsRoute longIcon = validRoute;
  longIcon.iconName =
      QString(SettingsRoute::MaximumIconNameLength + 1, QLatin1Char('i'));
  QVERIFY(!longIcon.isValid());
}

void SettingsRouteRegistryTest::testRegistryRegistrationAndLookup() {
  SettingsRouteRegistry registry;
  QCOMPARE(registry.count(), 0);
  QVERIFY(registry.isEmpty());

  SettingsRoute route1{
      .id = QStringLiteral("network"),
      .component = SettingsRouteComponent::Notifications,
      .title = QStringLiteral("Network"),
      .description = QStringLiteral("Network configuration"),
      .iconName = QStringLiteral("preferences-system-network"),
      .category = QStringLiteral("Connectivity"),
      .available = true,
      .unavailableReason = QString(),
  };

  QString error;
  QVERIFY(registry.registerRoute(route1, &error));
  QVERIFY(error.isEmpty());
  QCOMPARE(registry.count(), 1);
  QVERIFY(!registry.isEmpty());
  QVERIFY(registry.hasRoute(QStringLiteral("network")));
  QVERIFY(!registry.hasRoute(QStringLiteral("bluetooth")));

  const auto found = registry.route(QStringLiteral("network"));
  QVERIFY(found.has_value());
  QCOMPARE(found->id, QStringLiteral("network"));
  QCOMPARE(found->title, QStringLiteral("Network"));
  QCOMPARE(registry.indexOf(QStringLiteral("network")), 0);
  QVERIFY(registry.isRouteAvailable(QStringLiteral("network")));

  // Lookup of non-existent route
  const auto notFound = registry.route(QStringLiteral("unknown"));
  QVERIFY(!notFound.has_value());
  QCOMPARE(registry.indexOf(QStringLiteral("unknown")), -1);
  QVERIFY(!registry.isRouteAvailable(QStringLiteral("unknown")));
}

void SettingsRouteRegistryTest::testRegistryRejectsDuplicates() {
  SettingsRouteRegistry registry;
  SettingsRoute route1{
      .id = QStringLiteral("sound"),
      .component = SettingsRouteComponent::Notifications,
      .title = QStringLiteral("Sound"),
      .description = QStringLiteral("Audio preferences"),
      .iconName = QStringLiteral("audio-card"),
      .category = QStringLiteral("Hardware"),
      .available = true,
      .unavailableReason = QString(),
  };
  QVERIFY(registry.registerRoute(route1));

  SettingsRoute duplicateRoute{
      .id = QStringLiteral("sound"),
      .component = SettingsRouteComponent::Appearance,
      .title = QStringLiteral("Duplicate Sound"),
      .description = QStringLiteral("Other"),
      .iconName = QStringLiteral("audio-other"),
      .category = QStringLiteral("Hardware"),
      .available = false,
      .unavailableReason = QStringLiteral("Disabled"),
  };

  QString error;
  QVERIFY(!registry.registerRoute(duplicateRoute, &error));
  QVERIFY(error.contains(QStringLiteral("Duplicate")));
  QCOMPARE(registry.count(), 1);
  // Original untouched
  QCOMPARE(registry.route(QStringLiteral("sound"))->title,
           QStringLiteral("Sound"));
}

void SettingsRouteRegistryTest::testRegistryCapacityEnforcement() {
  SettingsRouteRegistry registry;
  for (qsizetype i = 0; i < SettingsRouteRegistry::MaximumRouteCount; ++i) {
    SettingsRoute r{
        .id = QStringLiteral("route-%1").arg(i),
        .component = SettingsRouteComponent::Notifications,
        .title = QStringLiteral("Route %1").arg(i),
        .description = QString(),
        .iconName = QString(),
        .category = QString(),
        .available = true,
        .unavailableReason = QString(),
    };
    QVERIFY(registry.registerRoute(r));
  }
  QCOMPARE(registry.count(), SettingsRouteRegistry::MaximumRouteCount);

  SettingsRoute overflowRoute{
      .id = QStringLiteral("route-overflow"),
      .component = SettingsRouteComponent::Notifications,
      .title = QStringLiteral("Overflow"),
      .description = QString(),
      .iconName = QString(),
      .category = QString(),
      .available = true,
      .unavailableReason = QString(),
  };
  QString error;
  QVERIFY(!registry.registerRoute(overflowRoute, &error));
  QVERIFY(error.contains(QStringLiteral("capacity exceeded")));
  QCOMPARE(registry.count(), SettingsRouteRegistry::MaximumRouteCount);
}

void SettingsRouteRegistryTest::testBuiltInRoutesIntegrity() {
  SettingsRouteRegistry registry = SettingsRouteRegistry::createDefault();
  QCOMPARE(registry.count(), 3);

  QVERIFY(registry.hasRoute(QStringLiteral("notifications")));
  QVERIFY(registry.hasRoute(QStringLiteral("appearance")));
  QVERIFY(registry.hasRoute(QStringLiteral("display")));

  // Index ordering: notifications is 0, appearance is 1, display is 2
  QCOMPARE(registry.indexOf(QStringLiteral("notifications")), 0);
  QCOMPARE(registry.indexOf(QStringLiteral("appearance")), 1);
  QCOMPARE(registry.indexOf(QStringLiteral("display")), 2);

  const auto notif = registry.route(QStringLiteral("notifications"));
  QVERIFY(notif.has_value());
  QCOMPARE(notif->id, QStringLiteral("notifications"));
  QCOMPARE(notif->component, SettingsRouteComponent::Notifications);
  QVERIFY(!notif->title.isEmpty());
  QVERIFY(!notif->description.isEmpty());
  QVERIFY(notif->available);

  const auto app = registry.route(QStringLiteral("appearance"));
  QVERIFY(app.has_value());
  QCOMPARE(app->id, QStringLiteral("appearance"));
  QCOMPARE(app->component, SettingsRouteComponent::Appearance);
  QVERIFY(!app->title.isEmpty());
  QVERIFY(!app->description.isEmpty());
  QVERIFY(app->available);

  const auto disp = registry.route(QStringLiteral("display"));
  QVERIFY(disp.has_value());
  QCOMPARE(disp->id, QStringLiteral("display"));
  QCOMPARE(disp->component, SettingsRouteComponent::Display);
  QVERIFY(!disp->title.isEmpty());
  QVERIFY(!disp->description.isEmpty());
  QVERIFY(disp->available);
}

void SettingsRouteRegistryTest::testRouteVariantMapConversion() {
  SettingsRoute route{
      .id = QStringLiteral("display"),
      .component = SettingsRouteComponent::Appearance,
      .title = QStringLiteral("Displays"),
      .description = QStringLiteral("Resolution and scaling"),
      .iconName = QStringLiteral("video-display"),
      .category = QStringLiteral("Hardware"),
      .available = false,
      .unavailableReason = QStringLiteral("Service offline"),
  };

  const QVariantMap map = route.toVariantMap();
  QCOMPARE(map.value(QStringLiteral("id")).toString(),
           QStringLiteral("display"));
  QCOMPARE(map.value(QStringLiteral("component")).toString(),
           QStringLiteral("appearance"));
  QCOMPARE(map.value(QStringLiteral("title")).toString(),
           QStringLiteral("Displays"));
  QCOMPARE(map.value(QStringLiteral("description")).toString(),
           QStringLiteral("Resolution and scaling"));
  QCOMPARE(map.value(QStringLiteral("iconName")).toString(),
           QStringLiteral("video-display"));
  QCOMPARE(map.value(QStringLiteral("category")).toString(),
           QStringLiteral("Hardware"));
  QCOMPARE(map.value(QStringLiteral("available")).toBool(), false);
  QCOMPARE(map.value(QStringLiteral("unavailableReason")).toString(),
           QStringLiteral("Service offline"));
}

QTEST_MAIN(SettingsRouteRegistryTest)
#include "tst_settings_route_registry.moc"
