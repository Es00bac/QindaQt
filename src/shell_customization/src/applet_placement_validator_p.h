// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/applets/applet_manifest.h"
#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_customization/editing_result.h"

#include <QHash>
#include <QVector>

#include <optional>

namespace QindaQt::ShellCustomization {

// An editor session copies the manifest catalog into this immutable lookup.
// It proves only placement compatibility; applet_host remains responsible for
// trust, capability grants, and executable loading.
class AppletPlacementValidator final {
public:
    explicit AppletPlacementValidator(QVector<Applets::AppletManifest> manifests);

    [[nodiscard]] const EditingError &initializationError() const noexcept;
    [[nodiscard]] std::optional<EditingError> validatePlacement(
        const Profiles::AppletSpec &applet,
        const Profiles::PanelSpec &panel) const;
    [[nodiscard]] std::optional<EditingError> validatePanel(
        const Profiles::PanelSpec &panel) const;
    [[nodiscard]] static bool placementChanges(
        const Profiles::AppletSpec &applet,
        const Profiles::PanelSpec &source,
        const Profiles::PanelSpec &target);
    [[nodiscard]] static bool zonePlacementChanges(
        const QVariantMap &sourceSettings,
        const QVariantMap &targetSettings);

private:
    QHash<QString, Applets::AppletManifest> m_manifests;
    EditingError m_initializationError;
};

} // namespace QindaQt::ShellCustomization
