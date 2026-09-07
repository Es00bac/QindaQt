// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/workspaces/workspace.h"
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

namespace QindaQt::Workspaces {
namespace {
bool fail(QString *error, const QString &message) {
  if (error)
    *error = message;
  return false;
}
bool identifier(const QString &value) {
  static const QRegularExpression pattern(
      QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_.-]{0,127}$"));
  return pattern.match(value).hasMatch();
}
void collectSlots(const Core::LayoutNode &node, QSet<QString> &ids) {
  if (node.isLeaf())
    ids.insert(node.windowId());
  else {
    collectSlots(*node.firstChild(), ids);
    collectSlots(*node.secondChild(), ids);
  }
}
} // namespace
bool Workspace::validate(QString *error) const {
  if (error)
    error->clear();
  if (!identifier(id))
    return fail(error, QStringLiteral("Workspace identifier is invalid"));
  if (name.trimmed().isEmpty() || name.size() > 128)
    return fail(
        error, QStringLiteral("Give the workspace a name of 1–128 characters"));
  for (const auto character : name)
    if (character.isNull() || character == QLatin1Char('\n') ||
        character == QLatin1Char('\r'))
      return fail(error, QStringLiteral("Workspace name must fit on one line"));
  static const QRegularExpression colorPattern(
      QStringLiteral("^#[0-9A-Fa-f]{6}$"));
  if (!color.isEmpty() && !colorPattern.match(color).hasMatch())
    return fail(error, QStringLiteral("Choose a valid container color"));
  if (!layout.validate().valid)
    return fail(error, QStringLiteral("Workspace layout is invalid"));
  if (applicationSlots.size() < 2 || applicationSlots.size() > 128)
    return fail(error,
                QStringLiteral("A workspace needs 2–128 application slots"));
  QSet<QString> ids;
  for (const auto &slot : applicationSlots) {
    if (!identifier(slot.id) || ids.contains(slot.id) ||
        slot.label.trimmed().isEmpty() || slot.label.size() > 128 ||
        !identifier(slot.desktopEntryId) || slot.urls.size() > 32)
      return fail(error, QStringLiteral(
                             "Application slot is incomplete or duplicated"));
    ids.insert(slot.id);
    for (const auto &url : slot.urls)
      if (url.size() > 8192 || !QUrl(url, QUrl::StrictMode).isValid() ||
          QUrl(url).isRelative())
        return fail(error, QStringLiteral(
                               "Application location must be an absolute URL"));
  }
  QSet<QString> leaves;
  for (const auto &page : layout.pages())
    collectSlots(page.root(), leaves);
  if (leaves != ids)
    return fail(error,
                QStringLiteral("Layout and application slots do not match"));
  return true;
}
QJsonObject Workspace::toJson() const {
  QJsonArray applications;
  for (const auto &slot : applicationSlots)
    applications.append(QJsonObject{
        {QStringLiteral("id"), slot.id},
        {QStringLiteral("label"), slot.label},
        {QStringLiteral("desktopEntryId"), slot.desktopEntryId},
        {QStringLiteral("urls"), QJsonArray::fromStringList(slot.urls)}});
  return {{QStringLiteral("schemaVersion"), 1},
          {QStringLiteral("id"), id},
          {QStringLiteral("name"), name},
          {QStringLiteral("color"), color},
          {QStringLiteral("layout"), layout.toJson()},
          {QStringLiteral("applications"), applications}};
}
std::optional<Workspace> Workspace::fromJson(const QJsonObject &json,
                                             QString *error) {
  if (json.value(QStringLiteral("schemaVersion")).toDouble(-1) != 1 ||
      !json.value(QStringLiteral("applications")).isArray()) {
    fail(error, QStringLiteral("Unsupported workspace document"));
    return std::nullopt;
  }
  auto layout = Core::WindowContainer::fromJson(
      json.value(QStringLiteral("layout")).toObject(), error);
  if (!layout)
    return std::nullopt;
  Workspace result{json.value(QStringLiteral("id")).toString(),
                   json.value(QStringLiteral("name")).toString(),
                   *layout,
                   {},
                   json.value(QStringLiteral("color")).toString()};
  for (const auto &value :
       json.value(QStringLiteral("applications")).toArray()) {
    const auto object = value.toObject();
    if (!object.value(QStringLiteral("urls")).isArray()) {
      fail(error, QStringLiteral("Application locations are invalid"));
      return std::nullopt;
    }
    ApplicationSlot slot{
        object.value(QStringLiteral("id")).toString(),
        object.value(QStringLiteral("label")).toString(),
        object.value(QStringLiteral("desktopEntryId")).toString(),
        {}};
    for (const auto &url : object.value(QStringLiteral("urls")).toArray()) {
      if (!url.isString()) {
        fail(error, QStringLiteral("Application location is not text"));
        return std::nullopt;
      }
      slot.urls.append(url.toString());
    }
    result.applicationSlots.append(slot);
  }
  return result.validate(error) ? std::optional<Workspace>{result}
                                : std::nullopt;
}
} // namespace QindaQt::Workspaces
