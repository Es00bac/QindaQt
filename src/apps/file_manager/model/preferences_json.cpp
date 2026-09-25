// SPDX-License-Identifier: GPL-3.0-or-later
#include "preferences_json.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QSet>
#include <QStringList>

namespace QindaQt::Apps::FileManager::PreferencesJson {
namespace {

// AGENT-GUARD: these lists are the schemas. Adding a name to one without a
// new version and a migration makes every existing document Malformed on
// the next launch.
[[nodiscard]] QStringList version1Keys() {
  return {QStringLiteral("defaultViewMode"),       QStringLiteral("showHidden"),
          QStringLiteral("directoriesFirst"),      QStringLiteral("sortColumn"),
          QStringLiteral("sortDirection"),         QStringLiteral("iconSize"),
          QStringLiteral("discoverNearbyServers"), QStringLiteral("defaultConnectScheme"),
          QStringLiteral("confirmTrash")};
}

[[nodiscard]] QStringList version2Keys() {
  return version1Keys() + QStringList{QStringLiteral("groupBy"),
                                      QStringLiteral("detailsColumns"),
                                      QStringLiteral("relativeDates"),
                                      QStringLiteral("rowDensity"),
                                      QStringLiteral("showExtensions"),
                                      QStringLiteral("folderViews")};
}

[[nodiscard]] QStringList columnKeys() {
  return {QStringLiteral("key"), QStringLiteral("width")};
}

[[nodiscard]] QStringList folderViewKeys() {
  return {QStringLiteral("location"),      QStringLiteral("viewMode"),
          QStringLiteral("sortColumn"),    QStringLiteral("sortDirection"),
          QStringLiteral("groupBy"),       QStringLiteral("iconSize"),
          QStringLiteral("columns")};
}

[[nodiscard]] bool hasExactly(const QJsonObject &object, const QStringList &keys) {
  const QStringList present = object.keys();
  return QSet<QString>(present.cbegin(), present.cend()) ==
         QSet<QString>(keys.cbegin(), keys.cend());
}

// Reads typed values out of one object and remembers whether every one of
// them had the expected JSON type.
class Reader final {
public:
  explicit Reader(const QJsonObject &object) : m_object(object) {}

  [[nodiscard]] QString text(const char *key) {
    const QJsonValue value = m_object.value(QLatin1String(key));
    m_shaped = m_shaped && value.isString();
    return value.toString();
  }
  [[nodiscard]] bool flag(const char *key) {
    const QJsonValue value = m_object.value(QLatin1String(key));
    m_shaped = m_shaped && value.isBool();
    return value.toBool();
  }
  // A fractional or out-of-range number reads as -1, which every bounded
  // integer field refuses.
  [[nodiscard]] int integer(const char *key) {
    const QJsonValue value = m_object.value(QLatin1String(key));
    m_shaped = m_shaped && value.isDouble();
    return value.toInt(-1);
  }
  [[nodiscard]] QJsonArray array(const char *key) {
    const QJsonValue value = m_object.value(QLatin1String(key));
    m_shaped = m_shaped && value.isArray();
    return value.toArray();
  }
  void refuse() { m_shaped = false; }
  [[nodiscard]] bool shaped() const { return m_shaped; }

private:
  const QJsonObject &m_object;
  bool m_shaped = true;
};

[[nodiscard]] QList<DetailsColumn> readColumns(const QJsonArray &array, Reader &outer) {
  QList<DetailsColumn> columns;
  for (const QJsonValue &item : array) {
    const QJsonObject object = item.toObject();
    if (!item.isObject() || !hasExactly(object, columnKeys())) {
      outer.refuse();
      return {};
    }
    Reader reader(object);
    DetailsColumn column{.key = reader.text("key"), .width = reader.integer("width")};
    if (!reader.shaped()) {
      outer.refuse();
      return {};
    }
    columns.append(column);
  }
  return columns;
}

[[nodiscard]] QList<RememberedFolderView> readFolderViews(const QJsonArray &array,
                                                          Reader &outer) {
  QList<RememberedFolderView> views;
  for (const QJsonValue &item : array) {
    const QJsonObject object = item.toObject();
    if (!item.isObject() || !hasExactly(object, folderViewKeys())) {
      outer.refuse();
      return {};
    }
    Reader reader(object);
    RememberedFolderView remembered;
    remembered.location = reader.text("location");
    remembered.view.viewMode = reader.text("viewMode");
    remembered.view.sortColumn = reader.text("sortColumn");
    remembered.view.sortDirection = reader.text("sortDirection");
    remembered.view.groupBy = reader.text("groupBy");
    remembered.view.iconSize = reader.integer("iconSize");
    remembered.view.columns = readColumns(reader.array("columns"), reader);
    if (!reader.shaped()) {
      outer.refuse();
      return {};
    }
    views.append(remembered);
  }
  return views;
}

[[nodiscard]] QJsonArray columnsToJson(const QList<DetailsColumn> &columns) {
  QJsonArray array;
  for (const DetailsColumn &column : columns) {
    array.append(QJsonObject{{QStringLiteral("key"), column.key},
                             {QStringLiteral("width"), column.width}});
  }
  return array;
}

std::optional<Preferences> refuse(QString *diagnostic, const char *message) {
  if (diagnostic != nullptr) {
    *diagnostic = QString::fromLatin1(message);
  }
  return std::nullopt;
}

} // namespace

QJsonObject encode(const Preferences &preferences) {
  QJsonArray folderViews;
  for (const RememberedFolderView &remembered : preferences.folderViews) {
    folderViews.append(QJsonObject{
        {QStringLiteral("location"), remembered.location},
        {QStringLiteral("viewMode"), remembered.view.viewMode},
        {QStringLiteral("sortColumn"), remembered.view.sortColumn},
        {QStringLiteral("sortDirection"), remembered.view.sortDirection},
        {QStringLiteral("groupBy"), remembered.view.groupBy},
        {QStringLiteral("iconSize"), remembered.view.iconSize},
        {QStringLiteral("columns"), columnsToJson(remembered.view.columns)},
    });
  }
  const QJsonObject values{
      {QStringLiteral("defaultViewMode"), preferences.defaultViewMode},
      {QStringLiteral("showHidden"), preferences.showHidden},
      {QStringLiteral("directoriesFirst"), preferences.directoriesFirst},
      {QStringLiteral("sortColumn"), preferences.sortColumn},
      {QStringLiteral("sortDirection"), preferences.sortDirection},
      {QStringLiteral("iconSize"), preferences.iconSize},
      {QStringLiteral("discoverNearbyServers"), preferences.discoverNearbyServers},
      {QStringLiteral("defaultConnectScheme"), preferences.defaultConnectScheme},
      {QStringLiteral("confirmTrash"), preferences.confirmTrash},
      {QStringLiteral("groupBy"), preferences.groupBy},
      {QStringLiteral("detailsColumns"), columnsToJson(preferences.detailsColumns)},
      {QStringLiteral("relativeDates"), preferences.relativeDates},
      {QStringLiteral("rowDensity"), preferences.rowDensity},
      {QStringLiteral("showExtensions"), preferences.showExtensions},
      {QStringLiteral("folderViews"), folderViews},
  };
  return {{QStringLiteral("version"), 2}, {QStringLiteral("preferences"), values}};
}

std::optional<Preferences> decode(const QJsonObject &document, int version,
                                  QString *diagnostic) {
  const bool knownVersion = version == 1 || version == 2;
  if (!knownVersion ||
      !hasExactly(document, {QStringLiteral("version"), QStringLiteral("preferences")}) ||
      document.value(QStringLiteral("version")).toDouble() != version ||
      !document.value(QStringLiteral("preferences")).isObject()) {
    return refuse(diagnostic, "Preference state has an invalid schema");
  }
  const QJsonObject values = document.value(QStringLiteral("preferences")).toObject();
  if (!hasExactly(values, version == 1 ? version1Keys() : version2Keys())) {
    return refuse(diagnostic, "Preference state has an invalid schema");
  }
  Reader reader(values);
  Preferences loaded;
  loaded.defaultViewMode = reader.text("defaultViewMode");
  loaded.showHidden = reader.flag("showHidden");
  loaded.directoriesFirst = reader.flag("directoriesFirst");
  loaded.sortColumn = reader.text("sortColumn");
  loaded.sortDirection = reader.text("sortDirection");
  loaded.iconSize = reader.integer("iconSize");
  loaded.discoverNearbyServers = reader.flag("discoverNearbyServers");
  loaded.defaultConnectScheme = reader.text("defaultConnectScheme");
  loaded.confirmTrash = reader.flag("confirmTrash");
  if (version == 2) {
    loaded.groupBy = reader.text("groupBy");
    loaded.detailsColumns = readColumns(reader.array("detailsColumns"), reader);
    loaded.relativeDates = reader.flag("relativeDates");
    loaded.rowDensity = reader.text("rowDensity");
    loaded.showExtensions = reader.flag("showExtensions");
    loaded.folderViews = readFolderViews(reader.array("folderViews"), reader);
  }
  if (!reader.shaped()) {
    return refuse(diagnostic, "Preference state has an invalid shape");
  }
  if (!loaded.isValid()) {
    return refuse(diagnostic, "Preference state contains an unknown value");
  }
  return loaded;
}

} // namespace QindaQt::Apps::FileManager::PreferencesJson
