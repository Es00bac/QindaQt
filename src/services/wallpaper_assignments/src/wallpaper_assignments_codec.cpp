// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/wallpaper_assignments/wallpaper_assignments.h"

#include <QMetaType>
#include <QVariantList>

#include <cmath>
#include <initializer_list>
#include <utility>

namespace QindaQt::Services::WallpaperAssignments {
namespace {

// Stored field names. AGENT-CONTRACT: this spelling is the persisted format
// documented in ADR-0286 and docs/wiki/shell/wallpapers.md; every writer and
// reader goes through this file, so changing one needs a new format version.
constexpr auto kVersion = "version";
constexpr auto kAssignments = "assignments";
constexpr auto kDisplay = "display";
constexpr auto kDesktop = "desktop";
constexpr auto kWallpaper = "wallpaper";

bool isString(const QVariant &value)
{
  return value.metaType().id() == QMetaType::QString;
}

// Settings1 normalizes JSON integers to qint64, but a value that crossed D-Bus
// or a JSON document may carry another integral type or an integral double;
// only the exact number 1 is the current format (the dock-items rule).
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
    return std::isfinite(number) && number == static_cast<double>(Bounds::formatVersion);
  }
  default:
    return false;
  }
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

std::optional<WallpaperAssignment> decodeRecord(const QVariant &value)
{
  if (value.metaType().id() != QMetaType::QVariantMap)
    return std::nullopt;
  const QVariantMap record = value.toMap();
  if (!hasExactly(record, {kDisplay, kDesktop, kWallpaper}))
    return std::nullopt;
  const QVariant display = record.value(QLatin1StringView(kDisplay));
  const QVariant desktop = record.value(QLatin1StringView(kDesktop));
  const QVariant wallpaper = record.value(QLatin1StringView(kWallpaper));
  if (!isString(display) || !isString(desktop) || !isString(wallpaper))
    return std::nullopt;
  return WallpaperAssignment{display.toString(), desktop.toString(), wallpaper.toString()};
}

WallpaperAssignmentsDecodeResult malformed(QString error)
{
  WallpaperAssignmentsDecodeResult result;
  result.error = std::move(error);
  return result;
}

} // namespace

WallpaperAssignmentsDecodeResult WallpaperAssignments::decodeSettingsValue(const QVariant &value)
{
  WallpaperAssignmentsDecodeResult none;
  none.value = WallpaperAssignments{};
  // Absent (an older Settings1 peer or document) and the schema default both
  // mean that every display follows the everywhere wallpaper.
  if (!value.isValid() || value.isNull() || value.metaType().id() == QMetaType::Nullptr)
    return none;
  if (value.metaType().id() != QMetaType::QVariantMap)
    return malformed(QStringLiteral("the stored wallpaper choices are not an object"));
  const QVariantMap document = value.toMap();
  if (document.isEmpty())
    return none;
  if (!hasExactly(document, {kVersion, kAssignments})
      || !isFormatVersion(document.value(QLatin1StringView(kVersion)))) {
    return malformed(QStringLiteral("the stored wallpaper choices have an unknown format"));
  }
  const QVariant list = document.value(QLatin1StringView(kAssignments));
  if (list.metaType().id() != QMetaType::QVariantList)
    return malformed(QStringLiteral("the stored wallpaper choices are not a list"));
  const QVariantList entries = list.toList();
  if (entries.size() > Bounds::maxAssignments)
    return malformed(QStringLiteral("too many stored wallpaper choices"));
  QList<WallpaperAssignment> records;
  records.reserve(entries.size());
  for (const QVariant &entry : entries) {
    const std::optional<WallpaperAssignment> record = decodeRecord(entry);
    if (!record)
      return malformed(QStringLiteral("a stored wallpaper choice is malformed"));
    records.append(*record);
  }
  std::optional<WallpaperAssignments> decoded = fromAssignments(records);
  if (!decoded) {
    return malformed(QStringLiteral(
        "a stored wallpaper choice repeats a scope or exceeds its limits"));
  }
  WallpaperAssignmentsDecodeResult result;
  result.value = std::move(decoded);
  return result;
}

QVariantMap WallpaperAssignments::encodeSettingsValue(const WallpaperAssignments &value)
{
  // AGENT-GUARD: no choices encode as the schema default `{}`, never as an
  // empty list, so "Revert to defaults" and "removed the last choice" are one
  // stored value and the Appearance draft diff stays clean.
  if (value.isEmpty())
    return {};
  QVariantList list;
  list.reserve(value.size());
  for (const WallpaperAssignment &record : value.assignments()) {
    list.append(QVariantMap{{QLatin1StringView(kDisplay), record.display},
                            {QLatin1StringView(kDesktop), record.desktop},
                            {QLatin1StringView(kWallpaper), record.wallpaper}});
  }
  return {{QLatin1StringView(kVersion), QVariant::fromValue(Bounds::formatVersion)},
          {QLatin1StringView(kAssignments), list}};
}

} // namespace QindaQt::Services::WallpaperAssignments
