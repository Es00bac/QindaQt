// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/dock_items/dock_items.h"

#include <QMetaType>
#include <QVariantList>

#include <cmath>
#include <initializer_list>
#include <utility>

namespace QindaQt::Services::DockItems {
namespace {

// Stored field names. AGENT-CONTRACT: this spelling is the persisted format
// documented in ADR-0265 and docs/wiki/shell/dock-items.md; every writer and reader
// goes through this file, so changing one needs a new format version.
constexpr auto kVersion = "version";
constexpr auto kItems = "items";
constexpr auto kKind = "kind";
constexpr auto kId = "id";
constexpr auto kPath = "path";
constexpr auto kName = "name";
constexpr auto kApplications = "applications";

QString kindName(DockItemKind kind)
{
  switch (kind) {
  case DockItemKind::Application:
    return QStringLiteral("application");
  case DockItemKind::Folder:
    return QStringLiteral("folder");
  case DockItemKind::File:
    return QStringLiteral("file");
  case DockItemKind::Group:
    return QStringLiteral("group");
  case DockItemKind::Trash:
    return QStringLiteral("trash");
  }
  return QStringLiteral("application");
}

std::optional<DockItemKind> kindFromName(const QString &name)
{
  if (name == QLatin1StringView("application"))
    return DockItemKind::Application;
  if (name == QLatin1StringView("folder"))
    return DockItemKind::Folder;
  if (name == QLatin1StringView("file"))
    return DockItemKind::File;
  if (name == QLatin1StringView("group"))
    return DockItemKind::Group;
  if (name == QLatin1StringView("trash"))
    return DockItemKind::Trash;
  return std::nullopt;
}

bool isString(const QVariant &value)
{
  return value.metaType().id() == QMetaType::QString;
}

// Settings1 normalizes JSON integers to qint64, but a value that crossed
// D-Bus or a JSON document may carry another integral type (or an integral
// double from QJsonValue); only the exact number 1 is the current format.
// (An integral conversion to qint64 yields 1 only from the value 1.)
bool isFormatVersion(const QVariant &value)
{
  switch (value.metaType().id()) {
  case QMetaType::Char:
  case QMetaType::SChar:
  case QMetaType::UChar:
  case QMetaType::Short:
  case QMetaType::UShort:
  case QMetaType::Int:
  case QMetaType::UInt:
  case QMetaType::Long:
  case QMetaType::ULong:
  case QMetaType::LongLong:
  case QMetaType::ULongLong:
    return value.toLongLong() == Bounds::formatVersion;
  case QMetaType::Double: {
    const double number = value.toDouble();
    return std::isfinite(number)
        && number == static_cast<double>(Bounds::formatVersion);
  }
  default:
    return false;
  }
}

// A string list may arrive as QStringList (a D-Bus `as`) or as a QVariantList
// of strings (JSON-native); anything else is malformed.
bool stringList(const QVariant &value, QStringList *strings)
{
  strings->clear();
  if (value.metaType().id() == QMetaType::QStringList) {
    *strings = value.toStringList();
    return true;
  }
  if (value.metaType().id() != QMetaType::QVariantList)
    return false;
  const QVariantList list = value.toList();
  if (list.size() > Bounds::maxGroupApplications)
    return false;
  for (const QVariant &element : list) {
    if (!isString(element))
      return false;
    strings->append(element.toString());
  }
  return true;
}

bool hasExactly(const QVariantMap &record, std::initializer_list<const char *> fields)
{
  if (record.size() != static_cast<qsizetype>(fields.size()))
    return false;
  for (const char *field : fields) {
    if (!record.contains(QLatin1StringView(field)))
      return false;
  }
  return true;
}

std::optional<DockItem> decodeItem(const QVariant &value)
{
  if (value.metaType().id() != QMetaType::QVariantMap)
    return std::nullopt;
  const QVariantMap record = value.toMap();
  const QVariant kindValue = record.value(QLatin1StringView(kKind));
  if (!isString(kindValue))
    return std::nullopt;
  const std::optional<DockItemKind> kind = kindFromName(kindValue.toString());
  if (!kind)
    return std::nullopt;
  DockItem item;
  switch (*kind) {
  case DockItemKind::Application: {
    const QVariant id = record.value(QLatin1StringView(kId));
    if (!hasExactly(record, {kKind, kId}) || !isString(id))
      return std::nullopt;
    item = DockItem::application(id.toString());
    break;
  }
  case DockItemKind::Folder:
  case DockItemKind::File: {
    const QVariant path = record.value(QLatin1StringView(kPath));
    if (!hasExactly(record, {kKind, kPath}) || !isString(path))
      return std::nullopt;
    item = *kind == DockItemKind::Folder ? DockItem::folder(path.toString())
                                         : DockItem::file(path.toString());
    break;
  }
  case DockItemKind::Group: {
    const QVariant name = record.value(QLatin1StringView(kName));
    QStringList members;
    if (!hasExactly(record, {kKind, kName, kApplications}) || !isString(name)
        || !stringList(record.value(QLatin1StringView(kApplications)), &members)) {
      return std::nullopt;
    }
    item = DockItem::group(name.toString(), members);
    break;
  }
  case DockItemKind::Trash:
    if (!hasExactly(record, {kKind}))
      return std::nullopt;
    item = DockItem::trash();
    break;
  }
  if (!DockItems::isValidItem(item))
    return std::nullopt;
  return item;
}

DockItemsDecodeResult malformed(const QString &error)
{
  DockItemsDecodeResult result;
  result.error = error;
  return result;
}

} // namespace

DockItemsDecodeResult DockItems::decodeSettingsValue(const QVariant &value)
{
  DockItemsDecodeResult unmigrated;
  unmigrated.items = DockItems{};
  unmigrated.unmigrated = true;
  if (!value.isValid() || value.isNull() || value.metaType().id() == QMetaType::Nullptr)
    return unmigrated;
  if (value.metaType().id() != QMetaType::QVariantMap)
    return malformed(QStringLiteral("the stored dock is not an object"));
  const QVariantMap document = value.toMap();
  if (document.isEmpty())
    return unmigrated;
  if (!hasExactly(document, {kVersion, kItems})
      || !isFormatVersion(document.value(QLatin1StringView(kVersion)))) {
    return malformed(QStringLiteral("the stored dock has an unknown format"));
  }
  const QVariant itemsValue = document.value(QLatin1StringView(kItems));
  if (itemsValue.metaType().id() != QMetaType::QVariantList)
    return malformed(QStringLiteral("the stored dock items are not a list"));
  const QVariantList list = itemsValue.toList();
  if (list.size() > Bounds::maxItems)
    return malformed(QStringLiteral("the stored dock has too many items"));
  QVector<DockItem> items;
  items.reserve(list.size());
  for (const QVariant &element : list) {
    const std::optional<DockItem> item = decodeItem(element);
    if (!item)
      return malformed(QStringLiteral("the stored dock has a malformed item"));
    items.append(*item);
  }
  std::optional<DockItems> dock = fromItems(items);
  if (!dock)
    return malformed(QStringLiteral("the stored dock repeats an item or exceeds its limits"));
  DockItemsDecodeResult result;
  result.items = std::move(dock);
  return result;
}

QVariantMap DockItems::encodeSettingsValue(const DockItems &dock)
{
  QVariantList list;
  list.reserve(dock.size());
  for (const DockItem &item : dock.items()) {
    QVariantMap record{{QLatin1StringView(kKind), kindName(item.kind)}};
    switch (item.kind) {
    case DockItemKind::Application:
      record.insert(QLatin1StringView(kId), item.applicationId);
      break;
    case DockItemKind::Folder:
    case DockItemKind::File:
      record.insert(QLatin1StringView(kPath), item.path);
      break;
    case DockItemKind::Group: {
      // A QVariantList of strings, never a QStringList: nested values must
      // stay JSON-native for the Settings1 document encoder (ADR-0076).
      QVariantList members;
      for (const QString &member : item.applications)
        members.append(member);
      record.insert(QLatin1StringView(kName), item.name);
      record.insert(QLatin1StringView(kApplications), members);
      break;
    }
    case DockItemKind::Trash:
      break;
    }
    list.append(record);
  }
  return {{QLatin1StringView(kVersion), QVariant::fromValue(Bounds::formatVersion)},
          {QLatin1StringView(kItems), list}};
}

std::optional<DockItems> DockItems::fromLegacyValue(const QVariant &stored)
{
  if (!stored.isValid() || stored.isNull() || stored.metaType().id() == QMetaType::Nullptr)
    return DockItems{};
  QStringList ids;
  if (stored.metaType().id() == QMetaType::QStringList) {
    ids = stored.toStringList();
  } else if (stored.metaType().id() == QMetaType::QVariantList) {
    for (const QVariant &element : stored.toList()) {
      if (!isString(element))
        return std::nullopt;
      ids.append(element.toString());
    }
  } else {
    return std::nullopt;
  }
  // AGENT-GUARD: ADR-0076's whole-list rule. One invalid, repeated, or
  // excess id poisons the legacy value, so hostile data never migrates.
  if (ids.size() > Bounds::maxLegacyPinned)
    return std::nullopt;
  for (qsizetype index = 0; index < ids.size(); ++index) {
    if (!ShellLauncher::Bounds::isValidEntryId(ids.at(index))
        || ids.indexOf(ids.at(index)) != index) {
      return std::nullopt;
    }
  }
  return fromLegacyPinned(ids);
}

DockItems DockItems::fromLegacyPinned(const QStringList &applicationIds)
{
  DockItems dock;
  for (const QString &id : applicationIds) {
    if (dock.size() >= Bounds::maxItems)
      break;
    // Invalid or repeated ids are skipped (fromLegacyValue has already
    // rejected a malformed stored list as a whole).
    (void)dock.insert(dock.size(), DockItem::application(id));
  }
  return dock;
}

std::optional<DockItems> DockItems::fromSettingsValues(const QVariantMap &values)
{
  const DockItemsDecodeResult decoded =
      decodeSettingsValue(values.value(QLatin1StringView(DockItemsSettingsKey)));
  if (!decoded.ok())
    return std::nullopt;
  if (!decoded.unmigrated)
    return decoded.items;
  return fromLegacyValue(values.value(QLatin1StringView(LegacyPinnedSettingsKey)));
}

} // namespace QindaQt::Services::DockItems
