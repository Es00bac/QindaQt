// SPDX-License-Identifier: LGPL-3.0-or-later
// ADR-0286: per-display and per-desktop wallpaper choices in the one
// Appearance draft. Edits validate through the shared codec and join the
// ordinary Apply/Revert boundary as the `appearance.wallpaperAssignments` key.
#include "qindaqt/apps/settings_appearance/appearance_settings_model.h"
#include "qindaqt/apps/settings_appearance/wallpaper_target_catalog.h"

#include <QFileInfo>

#include <utility>

namespace QindaQt::Apps::SettingsAppearance {
namespace {

using Services::WallpaperAssignments::ResolvedScope;
using Services::WallpaperAssignments::WallpaperAssignments;
using Services::WallpaperAssignments::WallpaperEditError;

QString scopeToken(ResolvedScope scope)
{
    switch (scope) {
    case ResolvedScope::DisplayDesktop: return QStringLiteral("display-desktop");
    case ResolvedScope::Display: return QStringLiteral("display");
    case ResolvedScope::Desktop: return QStringLiteral("desktop");
    case ResolvedScope::Everywhere: return QStringLiteral("everywhere");
    }
    return QStringLiteral("everywhere");
}

} // namespace

QObject *AppearanceSettingsModel::wallpaperTargets() const
{
    return m_wallpaperTargets.data();
}

void AppearanceSettingsModel::setWallpaperTargets(WallpaperTargetCatalog *targets)
{
    if (m_wallpaperTargets == targets) {
        return;
    }
    if (m_wallpaperTargets) {
        disconnect(m_wallpaperTargets.data(), nullptr, this, nullptr);
    }
    m_wallpaperTargets = targets;
    if (m_wallpaperTargets) {
        // Saved-choice rows carry display and desktop names, so they follow
        // hotplug and desktop renames as well as draft edits.
        connect(m_wallpaperTargets.data(), &WallpaperTargetCatalog::displaysChanged, this,
                &AppearanceSettingsModel::wallpaperAssignmentsChanged);
        connect(m_wallpaperTargets.data(), &WallpaperTargetCatalog::desktopsChanged, this,
                &AppearanceSettingsModel::wallpaperAssignmentsChanged);
    }
    Q_EMIT wallpaperTargetsChanged();
    Q_EMIT wallpaperAssignmentsChanged();
}

void AppearanceSettingsModel::noteConfirmedWallpaperAssignments(const QVariantMap &values)
{
    // AGENT-NOTE: AppearanceValues reads an unreadable stored value as "no
    // choices" so the route stays usable; this flag keeps that fact visible
    // until a valid value is confirmed (the next explicit choice replaces it).
    m_wallpaperAssignmentsUnreadable =
        !WallpaperAssignments::decodeSettingsValue(
             values.value(QLatin1String(AppearanceKeys::WallpaperAssignments)))
             .ok();
}

QString AppearanceSettingsModel::wallpaperAssignmentsNotice() const
{
    return m_wallpaperAssignmentsUnreadable
        ? tr("Saved wallpapers for individual displays and desktops could not be read, "
             "so every display shows the main wallpaper. Choosing a wallpaper for a "
             "display or desktop replaces them.")
        : QString();
}

bool AppearanceSettingsModel::setWallpaperFor(const QString &display, const QString &desktop,
                                              const QString &wallpaper)
{
    if (!canEdit()) {
        return false;
    }
    if (display.isEmpty() && desktop.isEmpty()) {
        return setDraftValue(QLatin1String(AppearanceKeys::Wallpaper), wallpaper);
    }
    WallpaperAssignments next = m_draft.wallpaperAssignments;
    if (next.set(display, desktop, wallpaper) != WallpaperEditError::None) {
        return false;
    }
    return setDraftValue(QLatin1String(AppearanceKeys::WallpaperAssignments),
                         WallpaperAssignments::encodeSettingsValue(next));
}

bool AppearanceSettingsModel::clearWallpaperFor(const QString &display, const QString &desktop)
{
    if (!canEdit() || (display.isEmpty() && desktop.isEmpty())) {
        return false;
    }
    WallpaperAssignments next = m_draft.wallpaperAssignments;
    if (!next.remove(display, desktop)) {
        return true;
    }
    return setDraftValue(QLatin1String(AppearanceKeys::WallpaperAssignments),
                         WallpaperAssignments::encodeSettingsValue(next));
}

QVariantMap AppearanceSettingsModel::wallpaperChoiceFor(const QString &display,
                                                        const QString &desktop) const
{
    const auto resolution =
        m_draft.wallpaperAssignments.resolve(m_draft.wallpaper, display, desktop);
    const bool everywhere = display.isEmpty() && desktop.isEmpty();
    const bool explicitChoice =
        everywhere || m_draft.wallpaperAssignments.find(display, desktop).has_value();
    return {{QStringLiteral("explicit"), explicitChoice},
            {QStringLiteral("value"), resolution.wallpaper},
            {QStringLiteral("scope"), scopeToken(resolution.scope)},
            {QStringLiteral("label"), wallpaperLabel(resolution.wallpaper)},
            {QStringLiteral("previewUrl"), previewUrlFor(resolution.wallpaper)}};
}

QVariantList AppearanceSettingsModel::wallpaperAssignmentRows() const
{
    QVariantList rows;
    for (const auto &choice : m_draft.wallpaperAssignments.assignments()) {
        const QString displayLabel = choice.display.isEmpty() || !m_wallpaperTargets
            ? QString()
            : m_wallpaperTargets->displayLabel(choice.display);
        const QString desktopName = choice.desktop.isEmpty() || !m_wallpaperTargets
            ? QString()
            : m_wallpaperTargets->desktopName(choice.desktop);
        // Unknown displays and desktops are named as absent, never guessed:
        // the choice stays saved and applies again when they return.
        rows.append(QVariantMap{
            {QStringLiteral("display"), choice.display},
            {QStringLiteral("desktop"), choice.desktop},
            {QStringLiteral("wallpaper"), choice.wallpaper},
            {QStringLiteral("displayPresent"), choice.display.isEmpty() || !displayLabel.isEmpty()},
            {QStringLiteral("desktopPresent"), choice.desktop.isEmpty() || !desktopName.isEmpty()},
            {QStringLiteral("displayLabel"),
             choice.display.isEmpty() ? tr("All displays")
             : displayLabel.isEmpty() ? tr("A display that is not connected")
                                      : displayLabel},
            {QStringLiteral("desktopLabel"),
             choice.desktop.isEmpty() ? tr("all desktops")
             : desktopName.isEmpty()  ? tr("a desktop that no longer exists")
                                      : desktopName},
            {QStringLiteral("wallpaperLabel"), wallpaperLabel(choice.wallpaper)}});
    }
    return rows;
}

QString AppearanceSettingsModel::wallpaperLabel(const QString &value) const
{
    if (value.isEmpty()) {
        return tr("No wallpaper");
    }
    QVariantList entries = m_bundledWallpapers;
    entries.append(m_userWallpaperCatalog.wallpapers());
    for (const QVariant &entry : std::as_const(entries)) {
        const QVariantMap map = entry.toMap();
        if (map.value(QStringLiteral("value")).toString() == value) {
            return map.value(QStringLiteral("name")).toString();
        }
    }
    static const QString bundledPrefix = QStringLiteral("qindaqt:");
    return value.startsWith(bundledPrefix) ? value.sliced(bundledPrefix.size())
                                           : QFileInfo(value).fileName();
}

} // namespace QindaQt::Apps::SettingsAppearance
