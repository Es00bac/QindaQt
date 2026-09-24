// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_launcher/launcher_bounds.h"

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

#include <optional>

namespace QindaQt::Services::DockItems {

// Settings1 keys (schema v2, `panels` domain). The dock value supersedes the
// application-id list of ADR-0076; that key is read once, as the migration
// source, and never written again (ADR-0265).
inline constexpr auto DockItemsSettingsKey = "panels.dockItems";
inline constexpr auto LegacyPinnedSettingsKey = "panels.launcherPinned";

// AGENT-CONTRACT: the hostile-input bounds of the stored dock value. Every
// writer (the shell dock, the Settings1DockPins helper other processes use)
// and every reader applies these through this one codec; a second copy of any
// limit would be a second authority that can drift. The application ceiling
// is the launcher's pinned ceiling because the launcher's Pinned section is a
// projection of the dock's applications.
namespace Bounds {
inline constexpr qint64 formatVersion = 1;
inline constexpr qsizetype maxItems = 32;
inline constexpr qsizetype maxApplications = ShellLauncher::Bounds::maxPinnedEntries;
inline constexpr qsizetype maxGroupApplications = 16;
inline constexpr qsizetype maxNameLength = 64;
// AGENT-GUARD: sized so the largest admissible dock (every item a path of
// this many UTF-16 units, three UTF-8 bytes each) stays inside Settings1's
// aggregate value bound (WireContract::MaximumAggregateValueBytes). A larger
// limit would admit docks the service then refuses to store.
inline constexpr qsizetype maxPathLength = 1024;
// ADR-0076's ceiling for the legacy id list. That list was never allowed to
// hold more, so a longer one is hostile and is not migrated.
inline constexpr qsizetype maxLegacyPinned = 16;
} // namespace Bounds

enum class DockItemKind {
  Application,
  Folder,
  File,
  Group,
  Trash,
};

// One top-level dock item. Only the fields of its kind are ever set:
// Application -> applicationId; Folder/File -> path (absolute, clean, never
// opened or resolved here: paths are data, not commands); Group -> name and
// 1..maxGroupApplications member application ids (one level: a group never
// contains a group); Trash -> nothing.
struct DockItem final {
  DockItemKind kind = DockItemKind::Application;
  QString applicationId;
  QString path;
  QString name;
  QStringList applications;

  [[nodiscard]] static DockItem application(const QString &id);
  [[nodiscard]] static DockItem folder(const QString &absolutePath);
  [[nodiscard]] static DockItem file(const QString &absolutePath);
  [[nodiscard]] static DockItem group(const QString &groupName,
                                      const QStringList &members);
  [[nodiscard]] static DockItem trash();

  [[nodiscard]] bool operator==(const DockItem &) const = default;
};

enum class DockEditError {
  None,
  // A malformed application id, path, or group name, or an empty group.
  InvalidItem,
  // The application, path, or Trash is already somewhere in the dock.
  AlreadyInDock,
  // The item or application ceiling would be exceeded.
  DockFull,
  // The group member ceiling would be exceeded.
  GroupFull,
  OutOfRange,
  // The operation needs an application (or a group) at that index.
  WrongKind,
  // The named application is not in the dock or not in that group.
  NotInDock,
};

class DockItems;

struct DockItemsDecodeResult;

// The ordered top-level dock items. A pure value: no Settings access, no
// filesystem, no process. Every edit validates first and leaves the value
// unchanged when it returns an error, so a caller can apply an edit to a copy
// and publish it only on success.
//
// AGENT-GUARD: an application appears at most once in the whole dock, a group
// member included ("one icon per app"); a path appears at most once; there is
// at most one Trash item. Edits that would break any of these return an
// error instead of silently deduplicating.
//
// Index conventions: `index` names an existing item in [0, size()). `gap` is
// an insertion point in [0, size()]: gap k lands before the item currently at
// k, and gap size() appends. Gaps are always measured on the list as it is
// before the edit.
class DockItems final {
public:
  [[nodiscard]] const QVector<DockItem> &items() const noexcept { return m_items; }
  [[nodiscard]] qsizetype size() const noexcept { return m_items.size(); }
  [[nodiscard]] bool isEmpty() const noexcept { return m_items.isEmpty(); }
  // Every application in dock order, group members in place of their group.
  [[nodiscard]] QStringList applicationIds() const;
  // Index of the top-level item holding the application (the item itself or
  // its group), or -1.
  [[nodiscard]] qsizetype indexOfApplication(const QString &applicationId) const;
  [[nodiscard]] qsizetype indexOfPath(const QString &path) const;
  [[nodiscard]] qsizetype indexOfTrash() const;

  DockEditError insert(qsizetype gap, const DockItem &item);
  // Inserts several items at one gap, in order, as one edit. Items already in
  // the dock are skipped; any other refusal leaves the value unchanged.
  // Returns AlreadyInDock when every item was already present.
  DockEditError insertAll(qsizetype gap, const QVector<DockItem> &items);
  DockEditError moveToGap(qsizetype from, qsizetype gap);
  DockEditError removeAt(qsizetype index);
  // Removes the application wherever it is; a group left empty is removed.
  DockEditError removeApplication(const QString &applicationId);
  // Dropping the application item at sourceIndex onto the item at
  // targetIndex: onto an application it makes a group named groupName of the
  // two (target first) in the target's place; onto a group it joins that
  // group. groupName is ignored when the target is already a group.
  DockEditError combine(qsizetype targetIndex, qsizetype sourceIndex,
                        const QString &groupName);
  // The same for an application not yet in the dock (an external drop).
  DockEditError combineWith(qsizetype targetIndex, const QString &applicationId,
                            const QString &groupName);
  // Wraps the application at index in a new one-member group ("New Group").
  DockEditError makeGroup(qsizetype index, const QString &groupName);
  DockEditError renameGroup(qsizetype index, const QString &groupName);
  // Replaces the group at index with its members as application items.
  DockEditError ungroup(qsizetype index);
  // Takes the member out of the group at groupIndex and inserts it as an
  // application item at gap; a group left empty is removed.
  DockEditError moveOutOfGroup(qsizetype groupIndex, const QString &applicationId,
                               qsizetype gap);

  [[nodiscard]] bool operator==(const DockItems &) const = default;

  // Strict codec for the Settings1 value (see ADR-0265 for the shape). A
  // malformed value, an unknown field, or any bound violation rejects the
  // whole value; partial dock state never enters a model. An absent value and
  // the schema default `{}` decode as `unmigrated`.
  [[nodiscard]] static DockItemsDecodeResult decodeSettingsValue(const QVariant &value);
  [[nodiscard]] static QVariantMap encodeSettingsValue(const DockItems &items);
  // The one-time migration from ADR-0076's id list: one application item per
  // valid, unique id in order, capped at maxItems.
  [[nodiscard]] static DockItems fromLegacyPinned(const QStringList &applicationIds);
  // The same from the stored legacy value: absent means an empty dock; a
  // malformed list (wrong type, invalid or repeated id, over ADR-0076's
  // maxLegacyPinned) is rejected as a whole and returns nothing.
  [[nodiscard]] static std::optional<DockItems> fromLegacyValue(const QVariant &stored);
  // The dock a Settings1 snapshot describes: the structured value when it is
  // migrated, else the legacy list. Nothing for a malformed structured value
  // or, while unmigrated, a malformed legacy list.
  [[nodiscard]] static std::optional<DockItems> fromSettingsValues(const QVariantMap &values);
  // Validates a complete item list against every rule above.
  [[nodiscard]] static std::optional<DockItems> fromItems(const QVector<DockItem> &items);

  [[nodiscard]] static bool isValidPath(const QString &path);
  [[nodiscard]] static bool isValidGroupName(const QString &name);
  [[nodiscard]] static bool isValidItem(const DockItem &item);

private:
  [[nodiscard]] qsizetype applicationCount() const;
  [[nodiscard]] DockEditError admissionError(const DockItem &item) const;

  QVector<DockItem> m_items;
};

struct DockItemsDecodeResult final {
  std::optional<DockItems> items;
  // True for an absent value or the schema default: the caller derives the
  // dock from the legacy key once and writes the structured value on the
  // first edit.
  bool unmigrated = false;
  QString error;

  [[nodiscard]] bool ok() const noexcept { return items.has_value(); }
};

} // namespace QindaQt::Services::DockItems
