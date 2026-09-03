// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_customize/customize_editor_host.h"

#include "qindaqt/shell_customization/layout_editing_repository.h"
#include "qindaqt/shell_customization_editor/coordinator_engine_adapter.h"
#include "qindaqt/shell_customization_editor/user_profile_store.h"

#include <utility>

namespace QindaQt::Apps::SettingsCustomize {

using ShellCustomization::LayoutEditingRepository;
using ShellCustomizationEditor::CoordinatorEditingEngine;
using ShellCustomizationEditor::EditorOutcome;
using ShellCustomizationEditor::EditorSession;

class RepositoryCustomizeEditorHost::Private final {
public:
    void initialize(LayoutEditingRepository &repository,
                    QVector<Applets::AppletManifest> manifests,
                    QString userProfileDirectory)
    {
        borrowedRepository = &repository;
        engine = std::make_unique<CoordinatorEditingEngine>(
            repository, std::move(manifests));
        session = std::make_unique<EditorSession>(
            *engine,
            ShellCustomizationEditor::UserProfileStore(
                std::move(userProfileDirectory)));
    }

    [[nodiscard]] std::shared_ptr<const ShellCustomization::LayoutEditingSnapshot>
    snapshot() const
    {
        lastSnapshot = engine ? engine->snapshot() : nullptr;
        return lastSnapshot;
    }

    std::unique_ptr<LayoutEditingRepository> ownedRepository;
    LayoutEditingRepository *borrowedRepository = nullptr;
    std::unique_ptr<CoordinatorEditingEngine> engine;
    std::unique_ptr<EditorSession> session;
    mutable std::shared_ptr<const ShellCustomization::LayoutEditingSnapshot>
        lastSnapshot;
};

RepositoryCustomizeEditorHost::RepositoryCustomizeEditorHost(
    Profiles::LayoutProfile profile,
    QVector<ShellLayout::LogicalOutput> outputs,
    QVector<Applets::AppletManifest> manifests,
    QString userProfileDirectory)
    : d(std::make_unique<Private>())
{
    d->ownedRepository = std::make_unique<LayoutEditingRepository>(
        std::move(profile), outputs, manifests);
    d->initialize(*d->ownedRepository, std::move(manifests),
                  std::move(userProfileDirectory));
}

RepositoryCustomizeEditorHost::RepositoryCustomizeEditorHost(
    LayoutEditingRepository &repository,
    QVector<Applets::AppletManifest> manifests,
    QString userProfileDirectory)
    : d(std::make_unique<Private>())
{
    d->initialize(repository, std::move(manifests),
                  std::move(userProfileDirectory));
}

RepositoryCustomizeEditorHost::~RepositoryCustomizeEditorHost() = default;

bool RepositoryCustomizeEditorHost::ready() const
{
    return d->engine && d->engine->isReady() && d->snapshot() != nullptr;
}

QString RepositoryCustomizeEditorHost::unavailableReason() const
{
    if (d->borrowedRepository == nullptr) {
        return QStringLiteral("the layout repository was not constructed");
    }
    if (!d->borrowedRepository->isReady()) {
        return d->borrowedRepository->initializationError().message;
    }
    if (!d->engine || !d->engine->isReady()) {
        return QStringLiteral("the layout editor lease is owned by another window");
    }
    if (d->snapshot() == nullptr) {
        return QStringLiteral("the layout repository has no validated snapshot");
    }
    return {};
}

const Profiles::LayoutProfile *RepositoryCustomizeEditorHost::profile() const
{
    const auto snapshot = d->snapshot();
    return snapshot ? &snapshot->profile : nullptr;
}

const ShellLayout::PanelLayoutResult *RepositoryCustomizeEditorHost::layout() const
{
    const auto snapshot = d->snapshot();
    return snapshot ? &snapshot->layout : nullptr;
}

bool RepositoryCustomizeEditorHost::dirty() const
{
    return d->session && d->session->isDirty();
}

bool RepositoryCustomizeEditorHost::canUndo() const
{
    return d->session && d->session->canUndo();
}

bool RepositoryCustomizeEditorHost::canRedo() const
{
    return d->session && d->session->canRedo();
}

bool RepositoryCustomizeEditorHost::visualDragActive() const
{
    return d->session && d->session->isVisualDragActive();
}

std::optional<ShellCustomizationEditor::DropAcceptance>
RepositoryCustomizeEditorHost::acceptance() const
{
    return d->session ? d->session->acceptance() : std::nullopt;
}

EditorOutcome RepositoryCustomizeEditorHost::arm(
    const ShellCustomizationEditor::DragPayload &payload)
{
    return d->session->armDrag(payload);
}

EditorOutcome RepositoryCustomizeEditorHost::beginDrag()
{
    return d->session->beginVisualDrag();
}

EditorOutcome RepositoryCustomizeEditorHost::hover(
    const ShellCustomizationEditor::DropTarget &target)
{
    return d->session->hoverTarget(target);
}

EditorOutcome RepositoryCustomizeEditorHost::drop()
{
    return d->session->drop();
}

EditorOutcome RepositoryCustomizeEditorHost::cancel()
{
    return d->session->cancelGesture();
}

EditorOutcome RepositoryCustomizeEditorHost::applyGesture(
    const ShellCustomizationEditor::CustomizationIntent &intent,
    const ShellCustomizationEditor::DropTarget &target,
    const QString &newAppletId)
{
    return d->session->applyGesture(intent, target, newAppletId);
}

EditorOutcome RepositoryCustomizeEditorHost::undo()
{
    return d->session->undo();
}

EditorOutcome RepositoryCustomizeEditorHost::redo()
{
    return d->session->redo();
}

EditorOutcome RepositoryCustomizeEditorHost::apply()
{
    return d->session->applyToUserProfile();
}

} // namespace QindaQt::Apps::SettingsCustomize
