// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/exporter/menu_exporter.h>

#include <QtCore/QUuid>

namespace QindaQt::Shell::GlobalMenu::Exporter
{

MenuExporter::MenuExporter(const MenuSource &source)
    : m_source(source)
{
}

ExportResult MenuExporter::refresh()
{
    Protocol::MenuTree candidate = m_source.snapshot();
    const Protocol::ValidationResult validation = Protocol::validateMenuTree(candidate);
    if (!validation.accepted) {
        // AGENT-GUARD: never overwrite m_lastAccepted on a rejected pull. A
        // transiently malformed source must not regress a previously good
        // menu out from under the applet/invocation-guard consumers.
        return ExportResult{.outcome = ExportOutcome::RejectedInvalid, .validation = validation};
    }

    const bool sameOwner = m_lastAccepted.has_value()
        && m_lastAccepted->ownerWindowId == candidate.ownerWindowId;

    candidate.epoch = sameOwner ? m_lastAccepted->epoch : QUuid::createUuid();

    const Protocol::MenuTree previousForDelta =
        m_lastAccepted.value_or(Protocol::MenuTree{});
    const Protocol::MenuTreeDelta delta = Protocol::computeMenuTreeDelta(previousForDelta, candidate);

    if (sameOwner && delta.identical()) {
        return ExportResult{.outcome = ExportOutcome::Unchanged, .validation = validation, .delta = delta};
    }

    candidate.revision = sameOwner ? m_lastAccepted->revision + 1 : 1;
    m_lastAccepted = candidate;
    return ExportResult{.outcome = ExportOutcome::Published, .validation = validation, .delta = delta};
}

std::optional<Protocol::MenuTree> MenuExporter::lastAccepted() const
{
    return m_lastAccepted;
}

} // namespace QindaQt::Shell::GlobalMenu::Exporter
