// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings_route.h"

namespace QindaQt::Apps::SettingsCenter {

bool isValidRouteId(const QString &id) noexcept {
  if (id.isEmpty() || id.size() > SettingsRoute::MaximumIdLength) {
    return false;
  }
  for (qsizetype index = 0; index < id.size(); ++index) {
    const QChar c = id.at(index);
    const char16_t u = c.unicode();
    const bool isAsciiAlnum =
        (u >= u'a' && u <= u'z') || (u >= u'0' && u <= u'9');
    const bool isSeparator = (u == u'-' || u == u'_');
    if (!isAsciiAlnum && (!isSeparator || index == 0)) {
      return false;
    }
  }
  return true;
}

bool isValidRouteComponent(SettingsRouteComponent component) noexcept {
  switch (component) {
  case SettingsRouteComponent::Notifications:
  case SettingsRouteComponent::Appearance:
    return true;
  }
  return false;
}

QString settingsRouteComponentKey(SettingsRouteComponent component) {
  switch (component) {
  case SettingsRouteComponent::Notifications:
    return QStringLiteral("notifications");
  case SettingsRouteComponent::Appearance:
    return QStringLiteral("appearance");
  }
  return {};
}

bool SettingsRoute::isValid() const noexcept {
  if (!isValidRouteId(id) || !isValidRouteComponent(component)) {
    return false;
  }
  if (title.contains(QChar::Null) || title.trimmed().isEmpty() ||
      title.size() > MaximumTitleLength) {
    return false;
  }
  if (description.contains(QChar::Null) ||
      description.size() > MaximumDescriptionLength) {
    return false;
  }
  if (iconName.contains(QChar::Null) ||
      iconName.size() > MaximumIconNameLength) {
    return false;
  }
  if (category.contains(QChar::Null) ||
      category.size() > MaximumCategoryLength) {
    return false;
  }
  if (unavailableReason.contains(QChar::Null) ||
      unavailableReason.size() > MaximumUnavailableReasonLength) {
    return false;
  }
  // AGENT-GUARD: Availability and its diagnostic are one truth value. A
  // hidden reason on an available route or an unavailable route with no
  // explanation would make navigation accessibility contradict the page.
  if (available != unavailableReason.trimmed().isEmpty()) {
    return false;
  }
  return true;
}

QVariantMap SettingsRoute::toVariantMap() const {
  return {
      {QStringLiteral("id"), id},
      {QStringLiteral("component"), settingsRouteComponentKey(component)},
      {QStringLiteral("title"), title},
      {QStringLiteral("description"), description},
      {QStringLiteral("iconName"), iconName},
      {QStringLiteral("category"), category},
      {QStringLiteral("available"), available},
      {QStringLiteral("unavailableReason"), unavailableReason},
  };
}

bool SettingsRoute::operator==(const SettingsRoute &other) const noexcept {
  return id == other.id && component == other.component &&
         title == other.title && description == other.description &&
         iconName == other.iconName && category == other.category &&
         available == other.available &&
         unavailableReason == other.unavailableReason;
}

} // namespace QindaQt::Apps::SettingsCenter
