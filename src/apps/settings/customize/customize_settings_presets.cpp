// SPDX-License-Identifier: LGPL-3.0-or-later
// The preset half of CustomizeSettingsModel (ADR-0267): the catalog
// projection and the user-store actions (save as, duplicate, rename, delete,
// restore). The Settings1 selection lives in customize_settings_model.cpp.
#include "qindaqt/apps/settings_customize/customize_settings_model.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace QindaQt::Apps::SettingsCustomize {
namespace {

QVariantMap presetProjection(const LayoutPreset &preset, const QString &activeId)
{
    // The card draws a miniature desktop from exactly these panel fields.
    QVariantList panels;
    panels.reserve(preset.profile.panels.size());
    for (const auto &panel : preset.profile.panels) {
        panels.append(QVariantMap{
            {QStringLiteral("id"), panel.id},
            {QStringLiteral("edge"), Profiles::toString(panel.edge)},
            {QStringLiteral("alignment"), Profiles::toString(panel.alignment)},
            {QStringLiteral("layer"), Profiles::toString(panel.layer)},
            {QStringLiteral("hideMode"), Profiles::toString(panel.hideMode)},
            {QStringLiteral("thickness"), panel.thickness},
            {QStringLiteral("length"), panel.length},
            {QStringLiteral("appletCount"), panel.applets.size()},
        });
    }
    return {
        {QStringLiteral("id"), preset.profile.id},
        {QStringLiteral("name"), preset.profile.name},
        {QStringLiteral("description"), preset.profile.description},
        {QStringLiteral("panels"), panels},
        {QStringLiteral("builtIn"), preset.builtIn},
        {QStringLiteral("own"), preset.own()},
        {QStringLiteral("modified"), preset.modified()},
        {QStringLiteral("active"), preset.profile.id == activeId},
        {QStringLiteral("isDefault"), preset.profile.id == DefaultLayoutPresetId},
    };
}

} // namespace

void CustomizeSettingsModel::reloadPresets()
{
    PresetCatalog catalog = loadPresetCatalog(m_locations.stockDirectories,
                                              m_locations.userDirectory);
    m_catalogError = catalog.error.left(512);
    m_presets = std::move(catalog.presets);
    refreshStoreWatch();
    updateState();
    Q_EMIT presetsChanged();
}

void CustomizeSettingsModel::refreshStoreWatch()
{
    const QString &directory = m_locations.userDirectory;
    if (directory.isEmpty()) {
        return;
    }
    // The same watch the shell keeps (a rewrite in place reports through
    // fileChanged, an add or removal through directoryChanged); before the
    // first save the store does not exist, so its parent is watched instead.
    QStringList paths;
    if (QFileInfo::exists(directory)) {
        paths.append(directory);
        const auto entries = QDir(directory).entryInfoList({QStringLiteral("*.json")},
                                                           QDir::Files);
        for (const QFileInfo &entry : entries) {
            paths.append(entry.absoluteFilePath());
        }
    } else if (const QString parent = QFileInfo(directory).absolutePath();
               QFileInfo::exists(parent)) {
        paths.append(parent);
    }
    QStringList watched = m_storeWatch.directories() + m_storeWatch.files();
    QStringList wanted = paths;
    watched.sort();
    wanted.sort();
    if (watched == wanted) {
        return;
    }
    if (!watched.isEmpty()) {
        m_storeWatch.removePaths(watched);
    }
    if (!paths.isEmpty()) {
        m_storeWatch.addPaths(paths);
    }
}

QVariantList CustomizeSettingsModel::presets() const
{
    QVariantList builtIns;
    QVariantList own;
    for (const LayoutPreset &preset : m_presets) {
        (preset.builtIn ? builtIns : own).append(presetProjection(preset, m_activeId));
    }
    std::sort(own.begin(), own.end(), [](const QVariant &left, const QVariant &right) {
        const QVariantMap a = left.toMap();
        const QVariantMap b = right.toMap();
        const int byName = a.value(QStringLiteral("name")).toString().localeAwareCompare(
            b.value(QStringLiteral("name")).toString());
        return byName != 0 ? byName < 0
                           : a.value(QStringLiteral("id")).toString()
                < b.value(QStringLiteral("id")).toString();
    });
    return builtIns + own;
}

const LayoutPreset *CustomizeSettingsModel::findPreset(const QString &presetId) const
{
    for (const LayoutPreset &preset : m_presets) {
        if (preset.profile.id == presetId) {
            return &preset;
        }
    }
    return nullptr;
}

QString CustomizeSettingsModel::presetName(const QString &presetId) const
{
    const LayoutPreset *preset = findPreset(presetId);
    return preset != nullptr && !preset->profile.name.isEmpty() ? preset->profile.name
                                                                : presetId;
}

int CustomizeSettingsModel::ownPresetCount() const
{
    return static_cast<int>(std::count_if(m_presets.cbegin(), m_presets.cend(),
                                          [](const LayoutPreset &preset) {
                                              return preset.own();
                                          }));
}

QString CustomizeSettingsModel::presetNameError(const QString &name,
                                                const QString &renamingId) const
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("Enter a name for the preset.");
    }
    if (trimmed.size() > MaximumPresetNameLength) {
        return QStringLiteral("Use at most %1 characters.").arg(MaximumPresetNameLength);
    }
    const auto codePoints = trimmed.toUcs4();
    if (!trimmed.isValidUtf16()
        || std::any_of(codePoints.cbegin(), codePoints.cend(),
                       [](char32_t codePoint) { return !QChar::isPrint(codePoint); })) {
        return QStringLiteral("Use only printable characters.");
    }
    for (const LayoutPreset &preset : m_presets) {
        if (preset.profile.id != renamingId
            && preset.profile.name.trimmed().compare(trimmed, Qt::CaseInsensitive) == 0) {
            return QStringLiteral("A preset named “%1” already exists.").arg(preset.profile.name);
        }
    }
    return {};
}

QString CustomizeSettingsModel::nextUserPresetId(const QString &name) const
{
    // A readable, file-name-safe id from the name. AGENT-GUARD: the "user-"
    // prefix keeps an own preset from ever taking an installed profile's id,
    // which would silently shadow that built-in instead of adding a preset.
    QString slug;
    for (const QChar character : name.trimmed().toLower()) {
        if (character.unicode() < 128 && character.isLetterOrNumber()) {
            slug.append(character);
        } else if (!slug.isEmpty() && !slug.endsWith(QLatin1Char('-'))) {
            slug.append(QLatin1Char('-'));
        }
    }
    slug.truncate(40);
    while (slug.endsWith(QLatin1Char('-'))) {
        slug.chop(1);
    }
    const QString base = QStringLiteral("user-")
        + (slug.isEmpty() ? QStringLiteral("preset") : slug);
    const QDir store(m_locations.userDirectory);
    for (int suffix = 1; suffix <= 1'000; ++suffix) {
        const QString candidate = suffix == 1
            ? base : base + QLatin1Char('-') + QString::number(suffix);
        if (findPreset(candidate) == nullptr
            && !QFileInfo::exists(store.filePath(
                Profiles::UserProfileStore::fileNameForId(candidate)))) {
            return candidate;
        }
    }
    return {};
}

QString CustomizeSettingsModel::copyName(const QString &name) const
{
    for (int number = 1; number <= 1'000; ++number) {
        const QString suffix = number == 1 ? QStringLiteral(" copy")
                                           : QStringLiteral(" copy %1").arg(number);
        const QString candidate =
            name.trimmed().left(MaximumPresetNameLength - suffix.size()).trimmed() + suffix;
        if (presetNameError(candidate).isEmpty()) {
            return candidate;
        }
    }
    return {};
}

QString CustomizeSettingsModel::fallbackPresetId(const QString &excludedId) const
{
    if (excludedId != DefaultLayoutPresetId
        && findPreset(QString(DefaultLayoutPresetId)) != nullptr) {
        return QString(DefaultLayoutPresetId);
    }
    for (const bool builtIn : {true, false}) {
        for (const LayoutPreset &preset : m_presets) {
            if (preset.builtIn == builtIn && preset.profile.id != excludedId) {
                return preset.profile.id;
            }
        }
    }
    return {};
}

bool CustomizeSettingsModel::copyPreset(const QString &sourceId, const QString &name)
{
    if (!canManage()) {
        report({}, QStringLiteral("Wait for Settings to finish updating, then try again."));
        return false;
    }
    // Re-read first: panel edits reach the user store while this page is
    // open, and a copy of the applied layout must include every one of them.
    reloadPresets();
    const LayoutPreset *source = findPreset(sourceId);
    if (source == nullptr) {
        report({}, QStringLiteral("That preset is no longer available."));
        return false;
    }
    if (const QString error = presetNameError(name); !error.isEmpty()) {
        report({}, error);
        return false;
    }
    if (ownPresetCount() >= MaximumUserPresets) {
        report({}, QStringLiteral("You can keep up to %1 presets. Delete one first.")
                       .arg(MaximumUserPresets));
        return false;
    }
    Profiles::LayoutProfile copy = source->profile;
    copy.id = nextUserPresetId(name);
    copy.name = name.trimmed();
    if (copy.id.isEmpty()) {
        report({}, QStringLiteral("Choose a different name for the preset."));
        return false;
    }
    const Profiles::UserProfileStoreResult saved = m_store.save(copy);
    reloadPresets();
    if (!saved.ok()) {
        report({}, QStringLiteral("The preset could not be saved: %1").arg(saved.message));
        return false;
    }
    report(QStringLiteral("Saved “%1”.").arg(copy.name));
    return true;
}

bool CustomizeSettingsModel::savePresetAs(const QString &sourceId, const QString &name)
{
    return copyPreset(sourceId, name);
}

bool CustomizeSettingsModel::duplicatePreset(const QString &presetId)
{
    const LayoutPreset *preset = findPreset(presetId);
    if (preset == nullptr || !preset->own()) {
        report({}, QStringLiteral("Only your own presets can be duplicated."));
        return false;
    }
    const QString name = copyName(preset->profile.name);
    if (name.isEmpty()) {
        report({}, QStringLiteral("Rename one of the copies first."));
        return false;
    }
    return copyPreset(presetId, name);
}

bool CustomizeSettingsModel::renamePreset(const QString &presetId, const QString &name)
{
    if (!canManage()) {
        report({}, QStringLiteral("Wait for Settings to finish updating, then try again."));
        return false;
    }
    reloadPresets();
    const LayoutPreset *preset = findPreset(presetId);
    if (preset == nullptr || !preset->own()) {
        report({}, QStringLiteral("Only your own presets can be renamed."));
        return false;
    }
    if (const QString error = presetNameError(name, presetId); !error.isEmpty()) {
        report({}, error);
        return false;
    }
    if (preset->profile.name == name.trimmed()) {
        report({}, {});
        return true;
    }
    // AGENT-NOTE: the id (and file name) stay, so a Settings1 selection that
    // names this preset stays valid. The content is the copy just re-read, so
    // only a panel edit landing between that read and this write can be lost.
    Profiles::LayoutProfile renamed = preset->profile;
    renamed.name = name.trimmed();
    const Profiles::UserProfileStoreResult saved = m_store.save(renamed);
    reloadPresets();
    if (!saved.ok()) {
        report({}, QStringLiteral("The preset could not be renamed: %1").arg(saved.message));
        return false;
    }
    report(QStringLiteral("Renamed to “%1”.").arg(renamed.name));
    return true;
}

bool CustomizeSettingsModel::deletePreset(const QString &presetId)
{
    if (!canManage()) {
        report({}, QStringLiteral("Wait for Settings to finish updating, then try again."));
        return false;
    }
    const LayoutPreset *preset = findPreset(presetId);
    if (preset == nullptr || !preset->own()) {
        report({}, QStringLiteral("Only your own presets can be deleted."));
        return false;
    }
    if (presetId != m_activeId) {
        return removeUserCopy(presetId, QStringLiteral("Deleted “%1”.")
                                            .arg(presetName(presetId)));
    }
    // AGENT-GUARD: switch first, delete after the switch is confirmed
    // (settleSelection). Deleting first would leave Settings1 naming a missing
    // layout, which this page and the shell's startup both treat as a fault.
    const QString fallback = fallbackPresetId(presetId);
    if (fallback.isEmpty()) {
        report({}, QStringLiteral("There is no other preset to switch to."));
        return false;
    }
    if (!canSwitch()) {
        report({}, QStringLiteral("Wait for Settings to finish updating, then try again."));
        return false;
    }
    report({}, {});
    return beginSelection(fallback, presetId);
}

bool CustomizeSettingsModel::restorePreset(const QString &presetId)
{
    if (!canManage()) {
        report({}, QStringLiteral("Wait for Settings to finish updating, then try again."));
        return false;
    }
    reloadPresets();
    const LayoutPreset *preset = findPreset(presetId);
    if (preset == nullptr || !preset->modified()) {
        report({}, QStringLiteral("Only an edited built-in layout can be restored."));
        return false;
    }
    const QString name = preset->original ? preset->original->name : presetName(presetId);
    return removeUserCopy(presetId, QStringLiteral("Restored the original “%1”.").arg(name));
}

bool CustomizeSettingsModel::removeUserCopy(const QString &presetId, const QString &notice)
{
    const QString name = presetName(presetId);
    const Profiles::UserProfileStoreResult removed = m_store.remove(presetId);
    reloadPresets();
    if (!removed.ok()) {
        report({}, QStringLiteral("“%1” could not be removed: %2").arg(name, removed.message));
        return false;
    }
    report(notice);
    return true;
}

} // namespace QindaQt::Apps::SettingsCustomize
