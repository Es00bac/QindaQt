// SPDX-License-Identifier: LGPL-3.0-or-later
#include "applet_placement_validator_p.h"

#include "qindaqt/applets/api_version.h"

#include <QJsonValue>
#include <QMetaType>

#include <utility>

namespace QindaQt::ShellCustomization {
namespace {

EditingError error(EditingErrorCode code,
                   QString message,
                   const QString &panelId = {},
                   const QString &appletId = {})
{
    return {code, std::move(message), panelId, appletId};
}

std::optional<Applets::Orientation> orientationForEdge(Profiles::Edge edge)
{
    switch (edge) {
    case Profiles::Edge::Top:
    case Profiles::Edge::Bottom:
        return Applets::Orientation::Horizontal;
    case Profiles::Edge::Left:
    case Profiles::Edge::Right:
        return Applets::Orientation::Vertical;
    }
    return std::nullopt;
}

std::optional<Applets::PlacementZone> zoneForSettings(const QVariantMap &settings)
{
    const auto found = settings.constFind(QStringLiteral("zone"));
    if (found == settings.cend()) {
        return Applets::PlacementZone::PanelStart;
    }
    QString token;
    if (found->metaType().id() == QMetaType::QString) {
        token = found->toString();
    } else if (found->metaType().id() == QMetaType::QJsonValue) {
        const QJsonValue value = found->value<QJsonValue>();
        if (!value.isString()) {
            return std::nullopt;
        }
        token = value.toString();
    } else {
        return std::nullopt;
    }
    if (token == QLatin1String("start")) {
        return Applets::PlacementZone::PanelStart;
    }
    if (token == QLatin1String("center")) {
        return Applets::PlacementZone::PanelCenter;
    }
    if (token == QLatin1String("end")) {
        return Applets::PlacementZone::PanelEnd;
    }
    return std::nullopt;
}

} // namespace

AppletPlacementValidator::AppletPlacementValidator(
    QVector<Applets::AppletManifest> manifests)
{
    for (Applets::AppletManifest &manifest : manifests) {
        const Applets::ManifestValidation validation = manifest.validate();
        if (!validation.isValid()) {
            m_initializationError = error(
                EditingErrorCode::InvalidManifest,
                QStringLiteral("manifest '%1' is invalid: %2")
                    .arg(manifest.id, validation.summary()));
            return;
        }
        if (m_manifests.contains(manifest.id)) {
            m_initializationError = error(
                EditingErrorCode::InvalidManifest,
                QStringLiteral("manifest catalog repeats plugin ID '%1'")
                    .arg(manifest.id));
            return;
        }
        m_manifests.insert(manifest.id, std::move(manifest));
    }
}

const EditingError &AppletPlacementValidator::initializationError() const noexcept
{
    return m_initializationError;
}

std::optional<EditingError> AppletPlacementValidator::validatePlacement(
    const Profiles::AppletSpec &applet,
    const Profiles::PanelSpec &panel) const
{
    const auto manifest = m_manifests.constFind(applet.plugin);
    if (manifest == m_manifests.cend()) {
        return error(EditingErrorCode::ManifestUnavailable,
                     QStringLiteral("manifest '%1' is unavailable in this editor session")
                         .arg(applet.plugin),
                     panel.id,
                     applet.id);
    }
    if (!manifest->supportsHost(Applets::ApiVersion::current())) {
        return error(EditingErrorCode::InvalidManifest,
                     QStringLiteral("manifest '%1' requires an unsupported host API")
                         .arg(applet.plugin),
                     panel.id,
                     applet.id);
    }

    const auto orientation = orientationForEdge(panel.edge);
    const auto zone = zoneForSettings(applet.settings);
    if (!orientation.has_value() || !zone.has_value()) {
        return error(EditingErrorCode::UnsupportedAppletPlacement,
                     QStringLiteral("applet '%1' has an invalid panel orientation or zone")
                         .arg(applet.id),
                     panel.id,
                     applet.id);
    }
    if (!manifest->orientations.contains(*orientation)
        || !manifest->placementZones.contains(*zone)) {
        return error(
            EditingErrorCode::UnsupportedAppletPlacement,
            QStringLiteral("applet '%1' does not support %2 placement in the panel-%3 zone")
                .arg(applet.id,
                     Applets::toString(*orientation),
                     Applets::toString(*zone).sliced(6)),
            panel.id,
            applet.id);
    }
    return std::nullopt;
}

std::optional<EditingError> AppletPlacementValidator::validatePanel(
    const Profiles::PanelSpec &panel) const
{
    for (const Profiles::AppletSpec &applet : panel.applets) {
        if (auto placementError = validatePlacement(applet, panel)) {
            return placementError;
        }
    }
    return std::nullopt;
}

bool AppletPlacementValidator::placementChanges(
    const Profiles::AppletSpec &applet,
    const Profiles::PanelSpec &source,
    const Profiles::PanelSpec &target)
{
    const auto sourceSignature =
        std::pair{orientationForEdge(source.edge), zoneForSettings(applet.settings)};
    const auto targetSignature =
        std::pair{orientationForEdge(target.edge), zoneForSettings(applet.settings)};
    return sourceSignature != targetSignature;
}

bool AppletPlacementValidator::zonePlacementChanges(
    const QVariantMap &sourceSettings,
    const QVariantMap &targetSettings)
{
    const auto sourceZone = zoneForSettings(sourceSettings);
    const auto targetZone = zoneForSettings(targetSettings);
    if (sourceZone.has_value() && targetZone.has_value()) {
        return sourceZone != targetZone;
    }

    const QString key = QStringLiteral("zone");
    return sourceSettings.contains(key) != targetSettings.contains(key)
        || sourceSettings.value(key) != targetSettings.value(key);
}

} // namespace QindaQt::ShellCustomization
