// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "applet_placement_validator_p.h"

#include "qindaqt/shell_customization/layout_editing_repository.h"

#include <QVector>

#include <memory>
#include <optional>
#include <utility>

namespace QindaQt::ShellCustomization {

struct LayoutEditingRepository::SessionState final {
    struct PreviewHistory final {
        std::shared_ptr<const Profiles::LayoutProfile> baseProfile;
        QVector<Profiles::LayoutProfile> undo;
        QVector<Profiles::LayoutProfile> redo;
    };

    explicit SessionState(QVector<Applets::AppletManifest> manifests,
                          quint64 revision)
        : placementValidator(std::move(manifests))
        , initialRevision(revision)
    {
    }

    AppletPlacementValidator placementValidator;
    std::shared_ptr<const LayoutEditingSnapshot> snapshot;
    EditingError initializationError;
    std::shared_ptr<const Profiles::LayoutProfile> committedProfile;
    QVector<Profiles::LayoutProfile> undo;
    QVector<Profiles::LayoutProfile> redo;
    std::optional<PreviewHistory> preview;
    // AGENT-GUARD: Update this before every preview snapshot publication and
    // clear it when preview ends; status() is intentionally allocation-free.
    bool previewDirty = false;
    // Retained separately because a failed initialization must not fabricate a
    // snapshot merely to preserve the caller's optimistic-revision boundary.
    quint64 initialRevision = 0;
    bool coordinatorAcquired = false;
    bool executing = false;
};

} // namespace QindaQt::ShellCustomization
