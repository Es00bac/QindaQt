// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_customize/customize_settings_model.h"

#include "qindaqt/shell_customization_editor/accessibility_identity.h"

#include <QJsonArray>
#include <QJsonObject>

namespace QindaQt::Apps::SettingsCustomize {
namespace {

const Profiles::PanelSpec *findPanel(const Profiles::LayoutProfile *profile,
                                     const QString &panelId)
{
    if (profile == nullptr) {
        return nullptr;
    }
    for (const auto &panel : profile->panels) {
        if (panel.id == panelId) {
            return &panel;
        }
    }
    return nullptr;
}

const Profiles::AppletSpec *findApplet(const Profiles::PanelSpec *panel,
                                       const QString &appletId)
{
    if (panel == nullptr) {
        return nullptr;
    }
    for (const auto &applet : panel->applets) {
        if (applet.id == appletId) {
            return &applet;
        }
    }
    return nullptr;
}

QVariantList schemaFields(const Applets::AppletManifest *manifest,
                          const Profiles::AppletSpec &applet)
{
    QVariantList fields;
    if (manifest == nullptr) {
        return fields;
    }
    const QJsonObject properties = manifest->settingsSchema
                                       .value(QStringLiteral("properties"))
                                       .toObject();
    QStringList names = properties.keys();
    names.sort();
    for (const QString &name : names) {
        const QJsonObject schema = properties.value(name).toObject();
        QVariantMap field{
            {QStringLiteral("key"), name},
            {QStringLiteral("label"), name},
            {QStringLiteral("type"), schema.value(QStringLiteral("type")).toString()},
            {QStringLiteral("editable"), false},
        };
        const QVariant current = applet.settings.value(name);
        field.insert(QStringLiteral("value"),
                     current.isValid()
                         ? current
                         : schema.value(QStringLiteral("default")).toVariant());
        field.insert(QStringLiteral("enumValues"),
                     schema.value(QStringLiteral("enum")).toArray().toVariantList());
        field.insert(QStringLiteral("minimum"),
                     schema.value(QStringLiteral("minimum")).toVariant());
        field.insert(QStringLiteral("maximum"),
                     schema.value(QStringLiteral("maximum")).toVariant());
        fields.append(field);
    }
    return fields;
}

} // namespace

QVariantList CustomizeSettingsModel::profiles() const
{
    QVariantList result;
    result.reserve(m_profiles.size());
    for (const auto &profile : m_profiles) {
        // The gallery draws every profile as a miniature desktop, not just
        // the selected one, so switching layouts is a visual choice. The
        // summary carries exactly the fields a miniature needs; applet-level
        // truth stays on the selected profile's full projection.
        QVariantList panelSummaries;
        panelSummaries.reserve(profile.panels.size());
        for (const auto &panel : profile.panels) {
            panelSummaries.append(QVariantMap{
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
        result.append(QVariantMap{
            {QStringLiteral("id"), profile.id},
            {QStringLiteral("name"), profile.name},
            {QStringLiteral("description"), profile.description},
            {QStringLiteral("panels"), panelSummaries},
        });
    }
    return result;
}

QVariantList CustomizeSettingsModel::palette() const
{
    QVariantList result;
    result.reserve(m_manifests.size());
    for (const auto &manifest : m_manifests) {
        result.append(QVariantMap{
            {QStringLiteral("id"), manifest.id},
            {QStringLiteral("name"), manifest.name},
            {QStringLiteral("description"), manifest.description},
            {QStringLiteral("settingsSchema"), manifest.settingsSchema.toVariantMap()},
        });
    }
    return result;
}

QVariantList CustomizeSettingsModel::panels() const
{
    QVariantList result;
    const auto *profile = m_editor ? m_editor->profile() : nullptr;
    const auto *layout = m_editor ? m_editor->layout() : nullptr;
    if (profile == nullptr || layout == nullptr) {
        return result;
    }
    result.reserve(profile->panels.size());
    for (const auto &panel : profile->panels) {
        QRect geometry;
        for (const auto &surface : layout->surfaces) {
            if (surface.panelId == panel.id) {
                geometry = surface.geometry;
                break;
            }
        }
        QVariantList applets;
        applets.reserve(panel.applets.size());
        int position = 0;
        for (const auto &applet : panel.applets) {
            ++position;
            const auto *manifest = findManifest(applet.plugin);
            applets.append(QVariantMap{
                {QStringLiteral("id"), applet.id},
                {QStringLiteral("pluginId"), applet.plugin},
                {QStringLiteral("name"), manifest ? manifest->name : applet.plugin},
                {QStringLiteral("zone"),
                 applet.settings.value(QStringLiteral("zone"),
                                       QStringLiteral("start"))},
                {QStringLiteral("position"), position},
                {QStringLiteral("count"), panel.applets.size()},
                {QStringLiteral("settings"), applet.settings},
            });
        }
        result.append(QVariantMap{
            {QStringLiteral("id"), panel.id},
            {QStringLiteral("name"),
             ShellCustomizationEditor::panelDisplayName(panel)},
            {QStringLiteral("output"), panel.output},
            {QStringLiteral("edge"), Profiles::toString(panel.edge)},
            {QStringLiteral("alignment"), Profiles::toString(panel.alignment)},
            {QStringLiteral("layer"), Profiles::toString(panel.layer)},
            {QStringLiteral("hideMode"), Profiles::toString(panel.hideMode)},
            {QStringLiteral("rows"), panel.rows},
            {QStringLiteral("thickness"), panel.thickness},
            {QStringLiteral("length"), panel.length},
            {QStringLiteral("x"), geometry.x()},
            {QStringLiteral("y"), geometry.y()},
            {QStringLiteral("width"), geometry.width()},
            {QStringLiteral("height"), geometry.height()},
            {QStringLiteral("applets"), applets},
        });
    }
    return result;
}

QVariantMap CustomizeSettingsModel::selectedProperties() const
{
    const auto *profile = m_editor ? m_editor->profile() : nullptr;
    const Profiles::PanelSpec *panel = findPanel(profile, m_selectedPanelId);
    if (m_selectedKind == QLatin1String("panel") && panel != nullptr) {
        return {
            {QStringLiteral("kind"), QStringLiteral("panel")},
            {QStringLiteral("name"),
             ShellCustomizationEditor::panelDisplayName(*panel)},
            {QStringLiteral("edge"), Profiles::toString(panel->edge)},
            {QStringLiteral("alignment"), Profiles::toString(panel->alignment)},
            {QStringLiteral("layer"), Profiles::toString(panel->layer)},
            {QStringLiteral("hideMode"), Profiles::toString(panel->hideMode)},
            {QStringLiteral("rows"), panel->rows},
            {QStringLiteral("thickness"), panel->thickness},
            {QStringLiteral("length"), panel->length},
        };
    }
    const Profiles::AppletSpec *applet = findApplet(panel, m_selectedAppletId);
    if (m_selectedKind == QLatin1String("applet") && applet != nullptr) {
        const auto *manifest = findManifest(applet->plugin);
        return {
            {QStringLiteral("kind"), QStringLiteral("applet")},
            {QStringLiteral("name"), manifest ? manifest->name : applet->plugin},
            {QStringLiteral("pluginId"), applet->plugin},
            {QStringLiteral("zone"),
             applet->settings.value(QStringLiteral("zone"),
                                    QStringLiteral("start"))},
            {QStringLiteral("settings"), applet->settings},
            {QStringLiteral("settingsFields"), schemaFields(manifest, *applet)},
            {QStringLiteral("schemaAvailable"), manifest != nullptr},
        };
    }
    return {};
}

void CustomizeSettingsModel::clearSelectionIfMissing()
{
    const auto *profile = m_editor ? m_editor->profile() : nullptr;
    const auto *panel = findPanel(profile, m_selectedPanelId);
    if (m_selectedKind == QLatin1String("panel") && panel != nullptr) {
        return;
    }
    if (m_selectedKind == QLatin1String("applet")
        && findApplet(panel, m_selectedAppletId) != nullptr) {
        return;
    }
    m_selectedKind.clear();
    m_selectedPanelId.clear();
    m_selectedAppletId.clear();
    Q_EMIT selectionChanged();
}

void CustomizeSettingsModel::refreshProjection()
{
    clearSelectionIfMissing();
    Q_EMIT contentChanged();
    Q_EMIT selectionChanged();
    Q_EMIT stateChanged();
}

} // namespace QindaQt::Apps::SettingsCustomize
