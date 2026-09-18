// SPDX-License-Identifier: GPL-3.0-or-later
#include "network/network_locations_controller.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QVariantMap>

using namespace QindaQt::Apps::FileManager;

namespace {

[[nodiscard]] QVariantMap sftpRequest(const QString &host, const QString &path,
                                      const QString &name = {},
                                      bool showInPlaces = true) {
  return {{QStringLiteral("scheme"), QStringLiteral("sftp")},
          {QStringLiteral("host"), host},
          {QStringLiteral("remotePath"), path},
          {QStringLiteral("displayName"), name},
          {QStringLiteral("showInPlaces"), showInPlaces}};
}

[[nodiscard]] std::unique_ptr<NetworkLocationsController> controllerIn(
    const QString &directory) {
  return std::make_unique<NetworkLocationsController>(
      std::make_unique<NetworkLocationsStore>(directory));
}

} // namespace

class TestNetworkLocationsController final : public QObject {
  Q_OBJECT

private slots:
  void savesPublishesAndPersists();
  void reSavingTheSameAddressUpdatesOneCard();
  void publishesOnlyPlacesLocationsInThePlacesList();
  void refusalKeepsTheListUnchangedAndExplains();
  void removeIgnoresOutOfRangeIndexes();
  void aRefusedWriteNeverChangesTheVisibleList();
};

void TestNetworkLocationsController::savesPublishesAndPersists() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString directory = temporary.filePath(QStringLiteral("state"));

  auto controller = controllerIn(directory);
  QSignalSpy changed(controller.get(), &NetworkLocationsController::locationsChanged);
  const QString url = controller->saveLocation(
      sftpRequest(QStringLiteral("qinda"), QStringLiteral("/mnt/storage"),
                  QStringLiteral("Storage (desktop)")));
  QCOMPARE(url, QStringLiteral("sftp://qinda/mnt/storage"));
  QCOMPARE(changed.count(), 1);
  QCOMPARE(controller->locations().size(), 1);
  const QVariantMap published = controller->locations().constFirst().toMap();
  QCOMPARE(published.value(QStringLiteral("name")).toString(),
           QStringLiteral("Storage (desktop)"));
  QCOMPARE(published.value(QStringLiteral("url")).toString(), url);
  QCOMPARE(published.value(QStringLiteral("index")).toInt(), 0);
  QVERIFY(controller->storeError().isEmpty());
  QVERIFY(controller->requestError().isEmpty());

  // A second controller over the same directory is the next launch.
  const auto reopened = controllerIn(directory);
  QCOMPARE(reopened->locationValues().size(), 1);
  QCOMPARE(reopened->locationValues().constFirst().url.toString(), url);
  QVERIFY(reopened->storeError().isEmpty());
}

void TestNetworkLocationsController::reSavingTheSameAddressUpdatesOneCard() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto controller = controllerIn(temporary.filePath(QStringLiteral("state")));

  QVERIFY(!controller->saveLocation(sftpRequest(QStringLiteral("qinda"),
                                                QStringLiteral("/mnt/storage"),
                                                QStringLiteral("First")))
               .isEmpty());
  QVERIFY(!controller->saveLocation(sftpRequest(QStringLiteral("qinda"),
                                                QStringLiteral("/mnt/storage"),
                                                QStringLiteral("Second"), false))
               .isEmpty());
  QCOMPARE(controller->locationValues().size(), 1);
  QCOMPARE(controller->locationValues().constFirst().name, QStringLiteral("Second"));
  QCOMPARE(controller->locationValues().constFirst().showInPlaces, false);
}

void TestNetworkLocationsController::publishesOnlyPlacesLocationsInThePlacesList() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto controller = controllerIn(temporary.filePath(QStringLiteral("state")));

  QVERIFY(!controller
               ->saveLocation(sftpRequest(QStringLiteral("qinda"),
                                          QStringLiteral("/mnt/storage"),
                                          QStringLiteral("Storage"), true))
               .isEmpty());
  QVERIFY(!controller
               ->saveLocation(sftpRequest(QStringLiteral("qinda"),
                                          QStringLiteral("/home/cabewse"),
                                          QStringLiteral("Home"), false))
               .isEmpty());
  QCOMPARE(controller->locations().size(), 2);
  QCOMPARE(controller->placesLocations().size(), 1);
  const QVariantMap place = controller->placesLocations().constFirst().toMap();
  QCOMPARE(place.value(QStringLiteral("name")).toString(), QStringLiteral("Storage"));
  // The published index addresses the full list, so removeLocation() from the
  // sidebar removes the row the user clicked and not a different one.
  QCOMPARE(place.value(QStringLiteral("index")).toInt(), 0);
}

void TestNetworkLocationsController::refusalKeepsTheListUnchangedAndExplains() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto controller = controllerIn(temporary.filePath(QStringLiteral("state")));
  QSignalSpy requestErrors(controller.get(),
                           &NetworkLocationsController::requestErrorChanged);

  QVERIFY(controller->saveLocation(sftpRequest(QStringLiteral("cabewse@qinda"),
                                               QStringLiteral("/mnt")))
              .isEmpty());
  QCOMPARE(requestErrors.count(), 1);
  QVERIFY(!controller->requestError().isEmpty());
  QVERIFY(controller->locations().isEmpty());

  // An accepted save clears the refusal, so the dialog stops explaining a
  // problem the user already fixed.
  QVERIFY(!controller
               ->saveLocation(sftpRequest(QStringLiteral("qinda"), QStringLiteral("/mnt")))
               .isEmpty());
  QVERIFY(controller->requestError().isEmpty());

  controller->clearRequestError();
  QVERIFY(controller->requestError().isEmpty());
}

void TestNetworkLocationsController::removeIgnoresOutOfRangeIndexes() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto controller = controllerIn(temporary.filePath(QStringLiteral("state")));
  QVERIFY(!controller
               ->saveLocation(sftpRequest(QStringLiteral("qinda"), QStringLiteral("/mnt")))
               .isEmpty());

  controller->removeLocation(-1);
  controller->removeLocation(9);
  QCOMPARE(controller->locationValues().size(), 1);
  controller->removeLocation(0);
  QVERIFY(controller->locationValues().isEmpty());
  QCOMPARE(controllerIn(temporary.filePath(QStringLiteral("state")))
               ->locationValues()
               .size(),
           0);
}

void TestNetworkLocationsController::aRefusedWriteNeverChangesTheVisibleList() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  // AGENT-GUARD: a state root the store refuses (a symlinked ancestor) must
  // leave the published list empty -- the sidebar may never show a location
  // the next launch would not find.
  const QString real = temporary.filePath(QStringLiteral("real"));
  QVERIFY(QDir().mkpath(real));
  const QString linked = temporary.filePath(QStringLiteral("linked"));
  QVERIFY(QFile::link(real, linked));

  auto controller = controllerIn(linked);
  QSignalSpy changed(controller.get(), &NetworkLocationsController::locationsChanged);
  QVERIFY(controller
              ->saveLocation(sftpRequest(QStringLiteral("qinda"), QStringLiteral("/mnt")))
              .isEmpty());
  QCOMPARE(changed.count(), 0);
  QVERIFY(controller->locations().isEmpty());
  QVERIFY(!controller->storeError().isEmpty());
  controller->clearStoreError();
  QVERIFY(controller->storeError().isEmpty());
}

QTEST_MAIN(TestNetworkLocationsController)
#include "tst_network_locations_controller.moc"
