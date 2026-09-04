// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_color_assignment/assignment_document.h>
#include <qindaqt/services/display_color_discovery/profile_discovery.h>
#include <qindaqt/services/display_protocol/display_types.h>

#include <QtCore/QVariantList>

namespace QindaQt::Apps::SettingsColor::Projection {

// Pure presentation projection for the Color route: bounded display strings
// and QML row values derived from the public Display1 snapshot, the confirmed
// Settings1 assignment document, and the C1 discovery catalog. No admission
// truth lives here — the model stamps "available" from its exact predicate.

[[nodiscard]] QString outputDisplayName(const QindaQt::Display::Output &output);
[[nodiscard]] QString originText(QindaQt::DisplayColor::ProfileOrigin origin);
[[nodiscard]] QString profileDisplayName(
    const QindaQt::DisplayColor::DiscoveryResult &catalog,
    const QString &profileId);

[[nodiscard]] QVariantList outputs(
    const QindaQt::Display::Snapshot &snapshot,
    const QindaQt::DisplayColor::AssignmentDocument &document,
    const QindaQt::DisplayColor::DiscoveryResult &catalog);

[[nodiscard]] QVariantList profiles(
    const QindaQt::DisplayColor::DiscoveryResult &catalog,
    const QString &assignedProfileId);

[[nodiscard]] QVariantList inactiveAssignments(
    const QindaQt::Display::Snapshot &snapshot,
    const QindaQt::DisplayColor::AssignmentDocument &document,
    const QindaQt::DisplayColor::DiscoveryResult &catalog);

} // namespace QindaQt::Apps::SettingsColor::Projection
