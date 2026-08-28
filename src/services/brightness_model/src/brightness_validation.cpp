// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/brightness_model/brightness_validation.h>

#include <qindaqt/services/brightness_model/brightness_limits.h>
#include <qindaqt/services/power_protocol/power_limits.h>
#include <qindaqt/services/power_protocol/power_validation.h>

#include <QtCore/QHash>
#include <QtCore/QSet>

namespace QindaQt::Brightness {
namespace {

FixtureValidationResult failed(const FixtureError error,
                               const char *reasonCode) {
  return {.error = error, .reasonCode = QString::fromLatin1(reasonCode)};
}

bool boundedRequiredText(const QString &value, const qsizetype maximum) {
  return !value.isEmpty() && Power::isBoundedText(value, maximum);
}

bool canonicalOptionalHandle(const Power::Handle &handle) {
  return handle.isValid() || (handle.epoch == 0 && handle.opaqueId.isEmpty());
}

QString handleKey(const Power::Handle &handle) {
  return QString::number(handle.epoch) + QLatin1Char(':') + handle.opaqueId;
}

} // namespace

FixtureValidationResult validateFixture(const FixtureSnapshot &snapshot) {
  if (snapshot.ownerAvailable) {
    if (!boundedRequiredText(snapshot.serviceEpoch,
                             kMaxFixtureEpochUtf8Bytes) ||
        snapshot.revision == 0) {
      return failed(FixtureError::InvalidLineage, "invalid-fixture-lineage");
    }
  } else if (!snapshot.serviceEpoch.isEmpty() || snapshot.revision != 0 ||
             !snapshot.displays.isEmpty()) {
    return failed(FixtureError::InvalidLineage, "stale-fixture-owner-loss");
  }
  if (snapshot.displays.size() > kMaxFixtureDisplays) {
    return failed(FixtureError::TooManyDisplays, "too-many-fixture-displays");
  }

  QHash<QString, const DisplayFixture *> byId;
  QSet<QString> mappedBacklights;
  for (const DisplayFixture &display : snapshot.displays) {
    if (!canonicalOptionalHandle(display.powerBacklightHandle)) {
      return failed(FixtureError::InvalidLineage,
                    "invalid-fixture-backlight-handle");
    }
    if (!boundedRequiredText(display.stableId, kMaxStableIdUtf8Bytes) ||
        !Power::isBoundedText(display.replicationSourceStableId,
                              kMaxStableIdUtf8Bytes) ||
        !Power::isBoundedText(display.powerBacklightHandle.opaqueId,
                              Power::kMaxOpaqueIdUtf8Bytes)) {
      return failed(FixtureError::InvalidText, "invalid-fixture-text");
    }
    if (byId.contains(display.stableId)) {
      return failed(FixtureError::DuplicateStableId,
                    "duplicate-fixture-stable-id");
    }
    if (!display.replicationSourceStableId.isEmpty() &&
        display.powerBacklightHandle.isValid()) {
      return failed(FixtureError::InvalidReplication,
                    "replica-has-backlight-mapping");
    }
    if (display.powerBacklightHandle.isValid()) {
      const QString key = handleKey(display.powerBacklightHandle);
      if (mappedBacklights.contains(key)) {
        return failed(FixtureError::DuplicateBacklightMapping,
                      "duplicate-fixture-backlight-mapping");
      }
      mappedBacklights.insert(key);
    }
    byId.insert(display.stableId, &display);
  }

  for (const DisplayFixture &display : snapshot.displays) {
    if (display.replicationSourceStableId.isEmpty()) {
      continue;
    }
    if (display.replicationSourceStableId == display.stableId ||
        !byId.contains(display.replicationSourceStableId)) {
      return failed(FixtureError::InvalidReplication,
                    "invalid-fixture-replication-source");
    }
    QSet<QString> visited;
    const DisplayFixture *cursor = &display;
    while (!cursor->replicationSourceStableId.isEmpty()) {
      if (visited.contains(cursor->stableId)) {
        return failed(FixtureError::InvalidReplication,
                      "fixture-replication-cycle");
      }
      visited.insert(cursor->stableId);
      cursor = byId.value(cursor->replicationSourceStableId, nullptr);
      if (cursor == nullptr) {
        return failed(FixtureError::InvalidReplication,
                      "invalid-fixture-replication-source");
      }
    }
  }
  return {};
}

} // namespace QindaQt::Brightness
