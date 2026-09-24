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
  case SettingsRouteComponent::Display:
  case SettingsRouteComponent::Network:
  case SettingsRouteComponent::Customize:
  case SettingsRouteComponent::Audio:
  case SettingsRouteComponent::Bluetooth:
  case SettingsRouteComponent::Power:
  case SettingsRouteComponent::Clipboard:
  case SettingsRouteComponent::Color:
  case SettingsRouteComponent::Accessibility:
  case SettingsRouteComponent::Input:
  case SettingsRouteComponent::Streaming:
  case SettingsRouteComponent::DateTime:
  case SettingsRouteComponent::Windows:
  case SettingsRouteComponent::DefaultApplications:
  case SettingsRouteComponent::AboutComputer:
  case SettingsRouteComponent::Startup:
  case SettingsRouteComponent::Screensaver:
  case SettingsRouteComponent::LoginScreen:
  case SettingsRouteComponent::Voice:
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
  case SettingsRouteComponent::Display:
    return QStringLiteral("display");
  case SettingsRouteComponent::Network:
    return QStringLiteral("network");
  case SettingsRouteComponent::Customize:
    return QStringLiteral("customize");
  case SettingsRouteComponent::Audio:
    return QStringLiteral("audio");
  case SettingsRouteComponent::Bluetooth:
    return QStringLiteral("bluetooth");
  case SettingsRouteComponent::Power:
    return QStringLiteral("power");
  case SettingsRouteComponent::Clipboard:
    return QStringLiteral("clipboard");
  case SettingsRouteComponent::Color:
    return QStringLiteral("color");
  case SettingsRouteComponent::Accessibility:
    return QStringLiteral("accessibility");
  case SettingsRouteComponent::Input:
    return QStringLiteral("input");
  case SettingsRouteComponent::Streaming:
    return QStringLiteral("streaming");
  case SettingsRouteComponent::DateTime:
    return QStringLiteral("datetime");
  case SettingsRouteComponent::Windows:
    return QStringLiteral("windows");
  case SettingsRouteComponent::DefaultApplications:
    return QStringLiteral("default-apps");
  case SettingsRouteComponent::AboutComputer:
    return QStringLiteral("about-computer");
  case SettingsRouteComponent::Startup:
    return QStringLiteral("startup");
  case SettingsRouteComponent::Screensaver:
    return QStringLiteral("screensaver");
  case SettingsRouteComponent::LoginScreen:
    return QStringLiteral("login-screen");
  case SettingsRouteComponent::Voice:
    return QStringLiteral("voice");
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
  if (!isValidSearchKeywords(keywords) ||
      destinations.size() > MaximumDestinationCount) {
    return false;
  }
  for (qsizetype index = 0; index < destinations.size(); ++index) {
    const SettingsRouteDestination &destination = destinations.at(index);
    if (!destination.isValid()) {
      return false;
    }
    // AGENT-GUARD: a destination id is what search hands to the page; two
    // entries with one id would list two results that open the same place.
    for (qsizetype earlier = 0; earlier < index; ++earlier) {
      if (destinations.at(earlier).id == destination.id) {
        return false;
      }
    }
  }
  return true;
}

QVariantMap SettingsRoute::toVariantMap() const {
  QVariantList destinationList;
  destinationList.reserve(destinations.size());
  for (const SettingsRouteDestination &destination : destinations) {
    destinationList.append(destination.toVariantMap());
  }
  return {
      {QStringLiteral("id"), id},
      {QStringLiteral("component"), settingsRouteComponentKey(component)},
      {QStringLiteral("title"), title},
      {QStringLiteral("description"), description},
      {QStringLiteral("iconName"), iconName},
      {QStringLiteral("category"), category},
      {QStringLiteral("available"), available},
      {QStringLiteral("unavailableReason"), unavailableReason},
      {QStringLiteral("keywords"), keywords},
      {QStringLiteral("destinations"), destinationList},
  };
}

bool SettingsRoute::operator==(const SettingsRoute &other) const noexcept {
  return id == other.id && component == other.component &&
         title == other.title && description == other.description &&
         iconName == other.iconName && category == other.category &&
         available == other.available &&
         unavailableReason == other.unavailableReason &&
         keywords == other.keywords && destinations == other.destinations;
}

bool isValidSearchKeywords(const QStringList &keywords) noexcept {
  if (keywords.size() > MaximumSearchKeywordCount) {
    return false;
  }
  for (const QString &keyword : keywords) {
    if (keyword.contains(QChar::Null) || keyword.trimmed().isEmpty() ||
        keyword.size() > MaximumSearchKeywordLength) {
      return false;
    }
  }
  return true;
}

bool SettingsRouteDestination::isValid() const noexcept {
  if (!isValidRouteId(id)) {
    return false;
  }
  if (title.contains(QChar::Null) || title.trimmed().isEmpty() ||
      title.size() > MaximumTitleLength) {
    return false;
  }
  return isValidSearchKeywords(keywords);
}

QVariantMap SettingsRouteDestination::toVariantMap() const {
  return {
      {QStringLiteral("id"), id},
      {QStringLiteral("title"), title},
      {QStringLiteral("keywords"), keywords},
  };
}

} // namespace QindaQt::Apps::SettingsCenter
