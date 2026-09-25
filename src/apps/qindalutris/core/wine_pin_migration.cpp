// SPDX-License-Identifier: GPL-3.0-or-later
#include "wine_pin_migration.h"

#include "proton_pin.h"

namespace QindaQt::QindaLutris {
namespace {

constexpr int kMaxMigrationNotes = 32;

QString describePin(const ProtonPin &pin, const QVector<ProtonBuild> &builds) {
  QString label = pin.name;
  const PinnedBuildResolution resolved = resolvePinnedBuild(pin, builds);
  if (resolved.ok()) {
    label = resolved.build->displayName;
  }
  return QStringLiteral("%1 (%2)").arg(label, protonVersionLabel(pin.version));
}

} // namespace

WinePinMigration migrateWineEntryPins(
    const QVector<WineEntryRecord> &records,
    const QHash<QString, LaunchOptions> &options,
    const QVector<ProtonBuild> &builds, const QString &preferredName) {
  WinePinMigration out;
  out.records = records;
  for (WineEntryRecord &record : out.records) {
    const LaunchOptions tuned =
        options.value(QStringLiteral("wine/") + record.slug);
    if (tuned.runnerOverride.value_or(record.runner) != WineRunner::Proton) {
      continue;
    }
    std::optional<ProtonPin> pin;
    if (record.protonPath.trimmed().isEmpty()) {
      pin = pinForNewEntry(QString(), builds, preferredName);
    } else if (record.protonVersion.trimmed().isEmpty()) {
      pin = confirmPinnedBuild(record.protonPath, builds);
    }
    if (!pin.has_value()) {
      continue;
    }
    record.protonPath = pin->name;
    record.protonVersion = pin->version;
    out.changed = true;
    if (out.notes.size() < kMaxMigrationNotes) {
      out.notes.append(QStringLiteral("%1 is now pinned to %2")
                           .arg(record.title, describePin(*pin, builds)));
    }
  }
  return out;
}

} // namespace QindaQt::QindaLutris
