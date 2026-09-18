// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_customization_editor/live_editor_host.h"

#include "qindaqt/shell_customization/layout_editing_repository.h"
#include "qindaqt/shell_customization_editor/coordinator_engine_adapter.h"
#include "qindaqt/shell_customization_editor/user_profile_store.h"

#include <utility>

namespace QindaQt::ShellCustomizationEditor {

using ShellCustomization::LayoutEditingRepository;

class LiveEditorHost::Private final {
public:
    // AGENT-GUARD: this composition order and these constructor arguments are
    // the parity contract with the Settings route's
    // RepositoryCustomizeEditorHost::Private::initialize. Do not add a
    // different engine, translator, or store here.
    void compose(Profiles::LayoutProfile profile,
                 QVector<ShellLayout::LogicalOutput> outputList)
    {
        session.reset();
        engine.reset();
        repository.reset();
        outputs = std::move(outputList);
        repository = std::make_unique<LayoutEditingRepository>(
            std::move(profile), outputs, manifests);
        engine = std::make_unique<CoordinatorEditingEngine>(*repository, manifests);
        session = std::make_unique<EditorSession>(
            *engine, UserProfileStore(userProfileDirectory));
    }

    [[nodiscard]] std::shared_ptr<const ShellCustomization::LayoutEditingSnapshot>
    snapshot() const
    {
        lastSnapshot = engine ? engine->snapshot() : nullptr;
        return lastSnapshot;
    }

    QVector<Applets::AppletManifest> manifests;
    QVector<ShellLayout::LogicalOutput> outputs;
    QString userProfileDirectory;
    std::unique_ptr<LayoutEditingRepository> repository;
    std::unique_ptr<CoordinatorEditingEngine> engine;
    std::unique_ptr<EditorSession> session;
    mutable std::shared_ptr<const ShellCustomization::LayoutEditingSnapshot> lastSnapshot;
};

LiveEditorHost::LiveEditorHost(Profiles::LayoutProfile profile,
                               QVector<ShellLayout::LogicalOutput> outputs,
                               QVector<Applets::AppletManifest> manifests,
                               QString userProfileDirectory)
    : d(std::make_unique<Private>())
{
    d->manifests = std::move(manifests);
    d->userProfileDirectory = std::move(userProfileDirectory);
    d->compose(std::move(profile), std::move(outputs));
}

LiveEditorHost::~LiveEditorHost() = default;

void LiveEditorHost::rebuild(Profiles::LayoutProfile profile,
                             QVector<ShellLayout::LogicalOutput> outputs)
{
    d->compose(std::move(profile), std::move(outputs));
}

bool LiveEditorHost::ready() const
{
    return d->engine && d->engine->isReady() && d->snapshot() != nullptr;
}

QString LiveEditorHost::unavailableReason() const
{
    if (!d->repository) {
        return QStringLiteral("the layout repository was not constructed");
    }
    if (!d->repository->isReady()) {
        return d->repository->initializationError().message;
    }
    if (!d->engine || !d->engine->isReady()) {
        return QStringLiteral("the layout editor lease is owned by another window");
    }
    if (d->snapshot() == nullptr) {
        return QStringLiteral("the layout repository has no validated snapshot");
    }
    return {};
}

bool LiveEditorHost::requiresRebuild() const
{
    return !d->session || d->session->requiresRebuild();
}

const Profiles::LayoutProfile *LiveEditorHost::profile() const
{
    const auto snapshot = d->snapshot();
    return snapshot ? &snapshot->profile : nullptr;
}

std::shared_ptr<const Profiles::LayoutProfile> LiveEditorHost::committedProfile() const
{
    return d->engine ? d->engine->committedProfile() : nullptr;
}

const ShellLayout::PanelLayoutResult *LiveEditorHost::layout() const
{
    const auto snapshot = d->snapshot();
    return snapshot ? &snapshot->layout : nullptr;
}

const QVector<Applets::AppletManifest> &LiveEditorHost::manifests() const noexcept
{
    return d->manifests;
}

const QVector<ShellLayout::LogicalOutput> &LiveEditorHost::outputs() const
{
    return d->outputs;
}

const QString &LiveEditorHost::userProfileDirectory() const noexcept
{
    return d->userProfileDirectory;
}

bool LiveEditorHost::dirty() const
{
    return d->session && d->session->isDirty();
}

bool LiveEditorHost::canUndo() const
{
    return d->session && d->session->canUndo();
}

bool LiveEditorHost::canRedo() const
{
    return d->session && d->session->canRedo();
}

bool LiveEditorHost::visualDragActive() const
{
    return d->session && d->session->isVisualDragActive();
}

std::optional<DropAcceptance> LiveEditorHost::acceptance() const
{
    return d->session ? d->session->acceptance() : std::nullopt;
}

EditorOutcome LiveEditorHost::arm(const DragPayload &payload)
{
    return d->session->armDrag(payload);
}

EditorOutcome LiveEditorHost::beginDrag()
{
    return d->session->beginVisualDrag();
}

EditorOutcome LiveEditorHost::hover(const DropTarget &target)
{
    return d->session->hoverTarget(target);
}

EditorOutcome LiveEditorHost::drop()
{
    return d->session->drop();
}

EditorOutcome LiveEditorHost::cancel()
{
    return d->session->cancelGesture();
}

EditorOutcome LiveEditorHost::applyGesture(const CustomizationIntent &intent,
                                           const DropTarget &target,
                                           const QString &newAppletId)
{
    return d->session->applyGesture(intent, target, newAppletId);
}

EditorOutcome LiveEditorHost::undo()
{
    return d->session->undo();
}

EditorOutcome LiveEditorHost::redo()
{
    return d->session->redo();
}

EditorOutcome LiveEditorHost::apply()
{
    return d->session->applyToUserProfile();
}

EditorOutcome LiveEditorHost::notifyOutputGenerationChanged()
{
    return d->session->notifyOutputGenerationChanged();
}

} // namespace QindaQt::ShellCustomizationEditor
