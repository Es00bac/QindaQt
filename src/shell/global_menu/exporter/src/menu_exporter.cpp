// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/exporter/menu_exporter.h>

namespace QindaQt::Shell::GlobalMenu::Exporter
{

namespace
{

// Content equality ignores lineage fields by construction: trees are compared
// through their item lists only, so a republished-but-identical menu under an
// advanced revision reports Unchanged content semantics via `changed=false`
// while still being stamped with the authoritative lineage.
bool sameContent(const Protocol::MenuTree &a, const Protocol::MenuTree &b)
{
    return a.ownerWindowId == b.ownerWindowId && a.items == b.items;
}

} // namespace

MenuExporter::MenuExporter(const MenuSource &source, const ExportLineageSource &lineageSource)
    : m_source(source)
    , m_lineageSource(lineageSource)
{
}

ExportResult MenuExporter::refresh()
{
    const MenuSnapshot snapshot = m_source.snapshot();
    if (!snapshot.complete) {
        // AGENT-GUARD: a source-reported incomplete traversal must never
        // reach validation or publication — its tree content is undefined
        // and publishing any prefix would misrepresent the application menu.
        return ExportResult{.outcome = ExportOutcome::RejectedIncomplete,
                             .defectCode = snapshot.defectCode};
    }

    Protocol::MenuTree candidate = snapshot.tree;
    const Protocol::ValidationResult validation = Protocol::validateMenuTree(candidate);
    if (!validation.accepted) {
        // AGENT-GUARD: never overwrite m_lastAccepted on a rejected pull. A
        // transiently malformed source must not regress a previously good
        // menu out from under the applet/invocation-guard consumers.
        return ExportResult{.outcome = ExportOutcome::RejectedInvalid, .validation = validation};
    }

    const std::optional<ExportLineage> lineage = m_lineageSource.lineageFor(candidate.ownerWindowId);
    if (!lineage) {
        return ExportResult{.outcome = ExportOutcome::RejectedNoAuthority};
    }

    candidate.epoch = lineage->epoch;
    candidate.revision = lineage->revision;

    const bool changed = !m_lastAccepted.has_value() || !sameContent(*m_lastAccepted, candidate);
    // AGENT-GUARD: even an unchanged menu must be re-stamped and stored: a
    // re-advanced lineage (a new adoption in the same epoch) has to reach
    // lastAccepted, or the exported tree would stay permanently stale against
    // the selector and every invocation would be rejected.
    m_lastAccepted = candidate;
    return ExportResult{.outcome = changed ? ExportOutcome::Published : ExportOutcome::Unchanged,
                         .validation = validation,
                         .changed = changed};
}

std::optional<Protocol::MenuTree> MenuExporter::lastAccepted() const
{
    return m_lastAccepted;
}

} // namespace QindaQt::Shell::GlobalMenu::Exporter
