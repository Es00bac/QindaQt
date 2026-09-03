// SPDX-License-Identifier: LGPL-3.0-or-later

#include "color_settings_projection.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QVariantMap>

namespace QindaQt::Apps::SettingsColor::Projection {
namespace {

using QindaQt::DisplayColor::AssignmentDocument;
using QindaQt::DisplayColor::DiscoveryResult;
using QindaQt::DisplayColor::IccProfileDescriptor;
using QindaQt::DisplayColor::ProfileOrigin;

QString tr(const char *text) {
  return QCoreApplication::translate("ColorSettings", text);
}

QString assignedProfileFor(const AssignmentDocument &document,
                           const QString &stableId) {
  for (const auto &record : document.records)
    if (record.outputStableId == stableId) return record.profileId;
  return {};
}

const IccProfileDescriptor *findProfile(const DiscoveryResult &catalog,
                                        const QString &profileId) {
  for (const auto &profile : catalog.profiles)
    if (profile.descriptor.profileId == profileId) return &profile.descriptor;
  return nullptr;
}

} // namespace

QString outputDisplayName(const QindaQt::Display::Output &output) {
  if (!output.label.trimmed().isEmpty()) return output.label.trimmed();
  QStringList parts;
  if (!output.manufacturer.trimmed().isEmpty())
    parts.append(output.manufacturer.trimmed());
  if (!output.model.trimmed().isEmpty()) parts.append(output.model.trimmed());
  if (!parts.isEmpty()) return parts.join(QLatin1Char(' '));
  if (!output.connectorName.trimmed().isEmpty()) return output.connectorName;
  return output.stableId;
}

QString originText(const ProfileOrigin origin) {
  switch (origin) {
  case ProfileOrigin::BuiltIn: return tr("Built-in profile");
  case ProfileOrigin::System: return tr("System profile");
  case ProfileOrigin::UserImported: return tr("Imported profile");
  case ProfileOrigin::EdidDerived: return tr("Display-reported profile");
  }
  return tr("Unknown origin");
}

QString profileDisplayName(const DiscoveryResult &catalog,
                           const QString &profileId) {
  if (const IccProfileDescriptor *profile = findProfile(catalog, profileId))
    return profile->displayName;
  return {};
}

QVariantList outputs(const QindaQt::Display::Snapshot &snapshot,
                     const AssignmentDocument &document,
                     const DiscoveryResult &catalog) {
  QVariantList rows;
  for (const QindaQt::Display::Output &output : snapshot.outputs) {
    const QString name = outputDisplayName(output);
    const QString assignedId = assignedProfileFor(document, output.stableId);
    const QString assignedName = assignedId.isEmpty()
        ? QString{} : profileDisplayName(catalog, assignedId);
    const QString assignmentText = assignedId.isEmpty()
        ? tr("No profile assigned")
        : assignedName.isEmpty()
          ? tr("Profile %1 (not in the discovered catalog)").arg(assignedId)
          : assignedName;
    rows.append(QVariantMap{
        {QStringLiteral("id"), output.stableId},
        {QStringLiteral("name"), name},
        {QStringLiteral("connectorName"), output.connectorName},
        {QStringLiteral("enabled"), output.enabled},
        {QStringLiteral("primary"), output.primary},
        {QStringLiteral("assigned"), !assignedId.isEmpty()},
        {QStringLiteral("assignedProfileId"), assignedId},
        {QStringLiteral("assignmentText"), assignmentText},
        {QStringLiteral("accessibleDescription"), assignedId.isEmpty()
             ? tr("%1, %2, no color profile assigned").arg(name, output.connectorName)
             : tr("%1, %2, assigned profile %3")
                   .arg(name, output.connectorName, assignmentText)},
    });
  }
  return rows;
}

QVariantList profiles(const DiscoveryResult &catalog,
                      const QString &assignedProfileId) {
  QVariantList rows;
  for (const auto &profile : catalog.profiles) {
    const IccProfileDescriptor &descriptor = profile.descriptor;
    const bool assigned = descriptor.profileId == assignedProfileId;
    const QString origin = originText(descriptor.origin);
    rows.append(QVariantMap{
        {QStringLiteral("id"), descriptor.profileId},
        {QStringLiteral("name"), descriptor.displayName},
        {QStringLiteral("description"), descriptor.description},
        {QStringLiteral("originText"), origin},
        {QStringLiteral("assigned"), assigned},
        {QStringLiteral("accessibleDescription"), assigned
             ? tr("%1, %2, currently assigned").arg(descriptor.displayName, origin)
             : tr("Assign %1, %2").arg(descriptor.displayName, origin)},
    });
  }
  return rows;
}

QVariantList inactiveAssignments(const QindaQt::Display::Snapshot &snapshot,
                                 const AssignmentDocument &document,
                                 const DiscoveryResult &catalog) {
  QVariantList rows;
  for (const auto &record : document.records) {
    bool connected = false;
    for (const QindaQt::Display::Output &output : snapshot.outputs)
      if (output.stableId == record.outputStableId) connected = true;
    if (connected) continue;
    const QString profileName = profileDisplayName(catalog, record.profileId);
    rows.append(QVariantMap{
        {QStringLiteral("id"), record.outputStableId},
        {QStringLiteral("profileId"), record.profileId},
        {QStringLiteral("profileText"), profileName.isEmpty()
             ? record.profileId : profileName},
        {QStringLiteral("accessibleDescription"),
         tr("Disconnected output %1 keeps assigned profile %2")
             .arg(record.outputStableId, record.profileId)},
    });
  }
  return rows;
}

} // namespace QindaQt::Apps::SettingsColor::Projection
