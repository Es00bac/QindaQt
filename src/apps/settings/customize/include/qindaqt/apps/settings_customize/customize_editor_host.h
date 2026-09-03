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

namespace QindaQt::ShellCustomization {
class LayoutEditingRepository;
}

namespace QindaQt::Apps::SettingsCustomize {

// View-model-facing ownership boundary around one repository, lease adapter,
// and EditorSession. It deliberately exposes EditorSession operations rather
// than the editing engine so Settings presentation cannot bypass gesture and
// rollback policy.
//
// AGENT-CONTRACT: All calls occur on the creating GUI thread. Query pointers
// are borrowed, nullable on unavailability, and valid only until the next host
// query or operation. Mutations report typed EditorOutcome failures and never
// throw or silently convert rejection into success.
class CustomizeEditorHost {
public:
    virtual ~CustomizeEditorHost() = default;

    [[nodiscard]] virtual bool ready() const = 0;
    [[nodiscard]] virtual QString unavailableReason() const = 0;
    [[nodiscard]] virtual const Profiles::LayoutProfile *profile() const = 0;
    [[nodiscard]] virtual const ShellLayout::PanelLayoutResult *layout() const = 0;
    [[nodiscard]] virtual bool dirty() const = 0;
    [[nodiscard]] virtual bool canUndo() const = 0;
    [[nodiscard]] virtual bool canRedo() const = 0;
    [[nodiscard]] virtual bool visualDragActive() const = 0;
    [[nodiscard]] virtual std::optional<ShellCustomizationEditor::DropAcceptance>
    acceptance() const = 0;

    [[nodiscard]] virtual ShellCustomizationEditor::EditorOutcome
    arm(const ShellCustomizationEditor::DragPayload &payload) = 0;
    [[nodiscard]] virtual ShellCustomizationEditor::EditorOutcome beginDrag() = 0;
    [[nodiscard]] virtual ShellCustomizationEditor::EditorOutcome
    hover(const ShellCustomizationEditor::DropTarget &target) = 0;
    [[nodiscard]] virtual ShellCustomizationEditor::EditorOutcome drop() = 0;
    [[nodiscard]] virtual ShellCustomizationEditor::EditorOutcome cancel() = 0;
    [[nodiscard]] virtual ShellCustomizationEditor::EditorOutcome
    applyGesture(const ShellCustomizationEditor::CustomizationIntent &intent,
                 const ShellCustomizationEditor::DropTarget &target,
                 const QString &newAppletId = {}) = 0;
    [[nodiscard]] virtual ShellCustomizationEditor::EditorOutcome undo() = 0;
    [[nodiscard]] virtual ShellCustomizationEditor::EditorOutcome redo() = 0;
    [[nodiscard]] virtual ShellCustomizationEditor::EditorOutcome apply() = 0;
};

// Production owner-thread composition. The borrowed-repository constructor is
// intentionally public so lease-loss behavior can be tested against the real
// coordinator contract without a service locator or test-only global. A
// borrowed repository must outlive this host; the value-taking constructor
// transfers profile/catalog state into a repository owned for the host's full
// lifetime.
class RepositoryCustomizeEditorHost final : public CustomizeEditorHost {
public:
    RepositoryCustomizeEditorHost(
        Profiles::LayoutProfile profile,
        QVector<ShellLayout::LogicalOutput> outputs,
        QVector<Applets::AppletManifest> manifests,
        QString userProfileDirectory);
    RepositoryCustomizeEditorHost(
        ShellCustomization::LayoutEditingRepository &repository,
        QVector<Applets::AppletManifest> manifests,
        QString userProfileDirectory);
    ~RepositoryCustomizeEditorHost() override;

    RepositoryCustomizeEditorHost(const RepositoryCustomizeEditorHost &) = delete;
    RepositoryCustomizeEditorHost &operator=(const RepositoryCustomizeEditorHost &) = delete;

    [[nodiscard]] bool ready() const override;
    [[nodiscard]] QString unavailableReason() const override;
    [[nodiscard]] const Profiles::LayoutProfile *profile() const override;
    [[nodiscard]] const ShellLayout::PanelLayoutResult *layout() const override;
    [[nodiscard]] bool dirty() const override;
    [[nodiscard]] bool canUndo() const override;
    [[nodiscard]] bool canRedo() const override;
    [[nodiscard]] bool visualDragActive() const override;
    [[nodiscard]] std::optional<ShellCustomizationEditor::DropAcceptance>
    acceptance() const override;
    [[nodiscard]] ShellCustomizationEditor::EditorOutcome
    arm(const ShellCustomizationEditor::DragPayload &payload) override;
    [[nodiscard]] ShellCustomizationEditor::EditorOutcome beginDrag() override;
    [[nodiscard]] ShellCustomizationEditor::EditorOutcome
    hover(const ShellCustomizationEditor::DropTarget &target) override;
    [[nodiscard]] ShellCustomizationEditor::EditorOutcome drop() override;
    [[nodiscard]] ShellCustomizationEditor::EditorOutcome cancel() override;
    [[nodiscard]] ShellCustomizationEditor::EditorOutcome
    applyGesture(const ShellCustomizationEditor::CustomizationIntent &intent,
                 const ShellCustomizationEditor::DropTarget &target,
                 const QString &newAppletId = {}) override;
    [[nodiscard]] ShellCustomizationEditor::EditorOutcome undo() override;
    [[nodiscard]] ShellCustomizationEditor::EditorOutcome redo() override;
    [[nodiscard]] ShellCustomizationEditor::EditorOutcome apply() override;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Apps::SettingsCustomize
