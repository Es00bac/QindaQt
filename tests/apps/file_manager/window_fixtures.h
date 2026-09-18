// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "model/preferences_controller.h"
#include "model/preferences_store.h"
#include "network/discovery_controller.h"
#include "network/network_locations_controller.h"
#include "network/network_locations_store.h"
#include "network/network_mount_manager.h"
#include "network/transfer_queue_controller.h"

#include <QVariant>
#include <QVariantMap>

#include <memory>
#include <utility>

namespace QindaQt::Apps::FileManager::Test {

// AGENT-CONTRACT: the five controllers every Main.qml harness must inject but
// which most rows do not exercise. Constructing them in one place keeps each
// harness from growing five lines every time the window gains a surface, and
// keeps their safe-for-tests wiring in one reviewable spot:
//
//  - the saved-location and preference stores live in the row's own temporary
//    directory, never in $XDG_STATE_HOME;
//  - the transfer queue has no worker, so nothing can be dispatched;
//  - discovery has no backend, so `supported` is false and no bus is touched;
//  - the mount manager has no systemd control and writes into the row's own
//    temporary directory, so no unit ever reaches the developer's own user
//    manager.
//
// A row that needs one of these to do something real composes that one itself
// instead of using this.
struct WindowSupportControllers final {
  explicit WindowSupportControllers(const QString &temporaryPath)
      : locations(std::make_unique<NetworkLocationsStore>(temporaryPath +
                                                          QStringLiteral("/state"))),
        transfers(nullptr),
        preferences(std::make_unique<PreferencesStore>(temporaryPath +
                                                       QStringLiteral("/state"))),
        discovery(nullptr),
        mounts(temporaryPath + QStringLiteral("/systemd"), temporaryPath, nullptr) {}

  void insertInto(QVariantMap &initialProperties) {
    const auto asObject = [](QObject *object) {
      return QVariant::fromValue(object);
    };
    initialProperties.insert(QStringLiteral("networkLocationsController"),
                             asObject(&locations));
    initialProperties.insert(QStringLiteral("transferQueueController"),
                             asObject(&transfers));
    initialProperties.insert(QStringLiteral("preferencesController"),
                             asObject(&preferences));
    initialProperties.insert(QStringLiteral("discoveryController"),
                             asObject(&discovery));
    initialProperties.insert(QStringLiteral("mountManager"), asObject(&mounts));
  }

  NetworkLocationsController locations;
  TransferQueueController transfers;
  PreferencesController preferences;
  DiscoveryController discovery;
  NetworkMountManager mounts;
};

} // namespace QindaQt::Apps::FileManager::Test
