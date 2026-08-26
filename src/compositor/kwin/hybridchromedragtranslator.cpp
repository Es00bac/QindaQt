// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridchromedragtranslator.h"

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

std::optional<HybridInput::InteractionIntent> reject(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return std::nullopt;
}

HybridInput::IntentPhase intentPhase(HybridChrome::DragPhase phase)
{
    switch (phase) {
    case HybridChrome::DragPhase::Begin:
        return HybridInput::IntentPhase::Begin;
    case HybridChrome::DragPhase::Update:
        return HybridInput::IntentPhase::Update;
    case HybridChrome::DragPhase::Commit:
        return HybridInput::IntentPhase::Commit;
    case HybridChrome::DragPhase::Cancel:
        return HybridInput::IntentPhase::Cancel;
    }
    Q_UNREACHABLE_RETURN(HybridInput::IntentPhase::Cancel);
}

QString firstWindowId(const Core::LayoutNode &node)
{
    const auto *current = &node;
    while (current->isSplit()) {
        current = current->firstChild();
    }
    return current->windowId();
}

bool memberBelongsTo(const Hybrid::WindowTopology &topology,
                     const QString &containerId,
                     const QString &windowId)
{
    const auto owner = topology.ownerOf(windowId);
    return owner && *owner == containerId;
}

} // namespace

HybridChromeDragTranslator::HybridChromeDragTranslator(
    const HybridInput::InteractionTargetResolver &targetResolver)
    : m_targetResolver(targetResolver)
{
}

std::optional<HybridInput::InteractionIntent> HybridChromeDragTranslator::translate(
    const Hybrid::WindowTopology &topology,
    const QString &containerId,
    const HybridChrome::ChromeDragEvent &event,
    QString *error) const
{
    if (error) {
        error->clear();
    }
    const auto *container = topology.container(containerId);
    if (!container) {
        return reject(error, QStringLiteral("chrome drag names an unknown container"));
    }

    HybridInput::InteractionIntent intent{
        .kind = HybridInput::InteractionKind::None,
        .phase = intentPhase(event.phase),
        .source = {},
        .target = {},
        .position = event.globalPosition,
        .delta = event.delta,
    };
    switch (event.target.kind) {
    case HybridChrome::HitKind::MemberTitleDrag:
        if (!memberBelongsTo(topology, containerId, event.target.stableId)) {
            return reject(error, QStringLiteral("member-title drag ownership is stale"));
        }
        intent.kind = HybridInput::InteractionKind::MemberDock;
        intent.source = {HybridInput::HitKind::MemberTitle,
                         containerId, event.target.stableId, {}};
        break;
    case HybridChrome::HitKind::Tab: {
        const auto *page = container->page(event.target.stableId);
        if (!page) {
            return reject(error, QStringLiteral("tab drag names an unknown page"));
        }
        intent.kind = HybridInput::InteractionKind::MemberDock;
        intent.source = {HybridInput::HitKind::Tab,
                         containerId, firstWindowId(page->root()), {}};
        intent.source.pageId = page->id();
        break;
    }
    case HybridChrome::HitKind::OuterTitleDrag:
        intent.kind = HybridInput::InteractionKind::ContainerMove;
        intent.source = {HybridInput::HitKind::OuterTitle, containerId, {}, {}};
        break;
    case HybridChrome::HitKind::Divider: {
        const auto *node = container->findNode(event.target.stableId);
        if (!node || !node->isSplit()) {
            return reject(error, QStringLiteral("divider drag names an unknown split"));
        }
        intent.kind = HybridInput::InteractionKind::DividerResize;
        intent.source = {HybridInput::HitKind::Divider,
                         containerId, {}, event.target.stableId};
        break;
    }
    case HybridChrome::HitKind::None:
    case HybridChrome::HitKind::WindowButton:
    case HybridChrome::HitKind::OuterResize:
    case HybridChrome::HitKind::Client:
        return reject(error,
                      QStringLiteral("chrome target is not a runtime drag interaction"));
    }

    if (intent.kind == HybridInput::InteractionKind::MemberDock
        && intent.phase != HybridInput::IntentPhase::Begin
        && intent.phase != HybridInput::IntentPhase::Cancel) {
        intent.target = m_targetResolver.pointerDockTarget(
            intent.source, event.globalPosition);
    }
    return intent;
}

} // namespace QindaQt::Compositor::KWinIntegration
