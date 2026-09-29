// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/wallpaper_target_catalog.h"

#include "qindaqt/services/wallpaper_assignments/wallpaper_assignments.h"

#include <qindaqt/services/display_client/client.h>

#include <utility>

namespace QindaQt::Apps::SettingsAppearance {
namespace {

using Services::WallpaperAssignments::WallpaperAssignments;

// Bounds the compositor already applies to its desktop list
// (Shell::Workspaces::Bounds); repeated here only as presentation caps.
constexpr qsizetype MaximumDesktops = 64;
constexpr qsizetype MaximumDesktopNameLength = 128;

// The monitor's own words: a user alias or label first, then its model, then
// the connector, so two displays are never both "Display".
QString outputName(const Display::Output &output)
{
    const QString label = output.label.trimmed();
    if (!label.isEmpty()) {
        return label;
    }
    const QString product =
        QStringLiteral("%1 %2").arg(output.manufacturer, output.model).trimmed();
    return product.isEmpty() ? output.connectorName : product;
}

} // namespace

WallpaperTargetCatalog::WallpaperTargetCatalog(QObject *parent)
    : QObject(parent)
{
}

WallpaperTargetCatalog::~WallpaperTargetCatalog() = default;

void WallpaperTargetCatalog::attachDisplayClient(DisplayClient::Client *client)
{
    if (m_client == client) {
        return;
    }
    if (m_client) {
        disconnect(m_client.data(), nullptr, this, nullptr);
    }
    m_client = client;
    if (m_client) {
        connect(m_client.data(), &DisplayClient::Client::snapshotChanged, this,
                &WallpaperTargetCatalog::refreshFromClient);
        // Owner loss drops the client's snapshot without a snapshot signal;
        // the catalog then shows no displays rather than stale ones.
        connect(m_client.data(), &DisplayClient::Client::stateChanged, this,
                &WallpaperTargetCatalog::refreshFromClient);
    }
    refreshFromClient();
}

void WallpaperTargetCatalog::refreshFromClient()
{
    const auto snapshot = m_client ? m_client->snapshot() : std::nullopt;
    setDisplayOutputs(snapshot ? snapshot->outputs : QList<Display::Output>{});
}

void WallpaperTargetCatalog::setDisplayOutputs(const QList<Display::Output> &outputs)
{
    QVariantList displays;
    QHash<QString, QString> labels;
    int ordinal = 0;
    for (const Display::Output &output : outputs) {
        ++ordinal;
        if (output.stableId.isEmpty()) {
            continue;
        }
        const QString name = outputName(output);
        const QString title = tr("Display %1").arg(ordinal);
        labels.insert(output.stableId, tr("%1 · %2").arg(title, name));
        if (!output.enabled || output.logicalSize.isEmpty()) {
            continue;
        }
        // AGENT-GUARD: an ambiguous ADR-0017 identity may swap between two
        // identical monitors, so it is shown but never offered as a scope.
        const bool assignable = !output.ambiguousIdentity
            && WallpaperAssignments::isValidDisplayId(output.stableId);
        displays.append(QVariantMap{
            {QStringLiteral("stableId"), output.stableId},
            {QStringLiteral("name"), name},
            {QStringLiteral("title"), title},
            {QStringLiteral("ordinal"), ordinal},
            {QStringLiteral("primary"), output.primary},
            {QStringLiteral("assignable"), assignable},
            {QStringLiteral("x"), output.position.x()},
            {QStringLiteral("y"), output.position.y()},
            {QStringLiteral("width"), output.logicalSize.width()},
            {QStringLiteral("height"), output.logicalSize.height()}});
    }
    if (displays == m_displays && labels == m_displayLabels) {
        return;
    }
    m_displays = std::move(displays);
    m_displayLabels = std::move(labels);
    Q_EMIT displaysChanged();
}

void WallpaperTargetCatalog::setDesktopRows(const QVariantList &rows)
{
    QVariantList desktops;
    QHash<QString, QString> names;
    for (const QVariant &row : rows) {
        const QVariantMap fields = row.toMap();
        const QString id = fields.value(QStringLiteral("id")).toString();
        if (!WallpaperAssignments::isValidDesktopId(id) || names.contains(id)) {
            continue;
        }
        QString name = fields.value(QStringLiteral("name")).toString().trimmed()
                           .left(MaximumDesktopNameLength);
        if (name.isEmpty()) {
            name = tr("Desktop %1").arg(desktops.size() + 1);
        }
        names.insert(id, name);
        desktops.append(QVariantMap{{QStringLiteral("id"), id},
                                    {QStringLiteral("name"), name}});
        if (desktops.size() >= MaximumDesktops) {
            break;
        }
    }
    if (desktops == m_desktops) {
        return;
    }
    m_desktops = std::move(desktops);
    m_desktopNames = std::move(names);
    Q_EMIT desktopsChanged();
}

QString WallpaperTargetCatalog::displayLabel(const QString &stableId) const
{
    return m_displayLabels.value(stableId);
}

QString WallpaperTargetCatalog::desktopName(const QString &desktopId) const
{
    return m_desktopNames.value(desktopId);
}

} // namespace QindaQt::Apps::SettingsAppearance
