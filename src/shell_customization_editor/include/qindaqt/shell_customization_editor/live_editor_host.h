// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/applets/applet_manifest.h"
#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_customization_editor/editor_intent.h"
#include "qindaqt/shell_customization_editor/editor_session.h"
#include "qindaqt/shell_layout/panel_layout_types.h"

#include <QString>
#include <QVector>

#include <memory>
#include <optional>

namespace QindaQt::ShellCustomizationEditor {

// The live customization host (ADR "host the customization editor live in the
// shell"). It composes exactly the trio the Settings Customize route's
// RepositoryCustomizeEditorHost composes -- LayoutEditingRepository ->
// CoordinatorEditingEngine -> EditorSession over the same UserProfileStore
// directory -- so a given intent sequence persists byte-identical profiles
// from either surface (the parity invariant). It owns no QML and no shell
// surface; the shell runtime maps menu entries and edit-mode gestures onto
// these calls and adopts the written profile through its store watcher.
//
// AGENT-CONTRACT: construct and call on one GUI thread. Query pointers are
// borrowed and valid only until the next host call. A host whose session
// reports requiresRebuild() must be rebuilt through rebuild() from the last
// applied profile before any further edit; the shell does this whenever it
// adopts a profile that differs from the session's committed profile.
class LiveEditorHost final {
public:
    LiveEditorHost(Profiles::LayoutProfile profile,
                   QVector<ShellLayout::LogicalOutput> outputs,
                   QVector<Applets::AppletManifest> manifests,
                   QString userProfileDirectory);
    ~LiveEditorHost();

    LiveEditorHost(const LiveEditorHost &) = delete;
    LiveEditorHost &operator=(const LiveEditorHost &) = delete;

    // Replaces the repository, engine and session from a new profile and
    // output inventory (the manifests and the store directory are kept).
    // Undo history does not survive a rebuild by design: the engine owns it.
    void rebuild(Profiles::LayoutProfile profile,
                 QVector<ShellLayout::LogicalOutput> outputs);

    [[nodiscard]] bool ready() const;
    [[nodiscard]] QString unavailableReason() const;
    [[nodiscard]] bool requiresRebuild() const;
    [[nodiscard]] const Profiles::LayoutProfile *profile() const;
    // The coordinator-retained committed profile (what Apply would write).
    [[nodiscard]] std::shared_ptr<const Profiles::LayoutProfile> committedProfile() const;
    [[nodiscard]] const ShellLayout::PanelLayoutResult *layout() const;
    [[nodiscard]] const QVector<Applets::AppletManifest> &manifests() const noexcept;
    [[nodiscard]] const QVector<ShellLayout::LogicalOutput> &outputs() const;
    [[nodiscard]] const QString &userProfileDirectory() const noexcept;
    [[nodiscard]] bool dirty() const;
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    [[nodiscard]] bool visualDragActive() const;
    [[nodiscard]] std::optional<DropAcceptance> acceptance() const;

    [[nodiscard]] EditorOutcome arm(const DragPayload &payload);
    [[nodiscard]] EditorOutcome beginDrag();
    [[nodiscard]] EditorOutcome hover(const DropTarget &target);
    [[nodiscard]] EditorOutcome drop();
    [[nodiscard]] EditorOutcome cancel();
    [[nodiscard]] EditorOutcome applyGesture(const CustomizationIntent &intent,
                                             const DropTarget &target,
                                             const QString &newAppletId = {});
    [[nodiscard]] EditorOutcome undo();
    [[nodiscard]] EditorOutcome redo();
    // Writes the committed profile to <userProfileDirectory>/<id>.json.
    [[nodiscard]] EditorOutcome apply();
    [[nodiscard]] EditorOutcome notifyOutputGenerationChanged();

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::ShellCustomizationEditor
