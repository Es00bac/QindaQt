// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/exporter/menu_source.h>
#include <qindaqt/shell/global_menu/protocol/menu_delta.h>
#include <qindaqt/shell/global_menu/protocol/menu_tree.h>
#include <qindaqt/shell/global_menu/protocol/menu_validation.h>

#include <optional>

namespace QindaQt::Shell::GlobalMenu::Exporter
{

enum class ExportOutcome {
    // The snapshot was valid and differs from the last accepted tree; it is
    // now authoritative.
    Published,
    // The snapshot was valid but structurally identical (after lineage
    // normalization) to the last accepted tree; nothing changed.
    Unchanged,
    // The snapshot failed validation. The last accepted tree, if any, is
    // untouched: a malformed pull never regresses a previously good menu,
    // matching the applet-manifest catalog's atomic-load convention.
    RejectedInvalid,
};

struct ExportResult final {
    ExportOutcome outcome = ExportOutcome::RejectedInvalid;
    Protocol::ValidationResult validation;
    Protocol::MenuTreeDelta delta;
};

// Pulls, validates, and assigns deterministic lineage to a MenuSource's
// snapshots. The exporter — not the source — owns epoch/revision assignment:
// the epoch persists and the revision advances while `ownerWindowId` stays
// the same; a new owner starts a fresh epoch at revision 1.
class MenuExporter final
{
public:
    explicit MenuExporter(const MenuSource &source);

    [[nodiscard]] ExportResult refresh();
    [[nodiscard]] std::optional<Protocol::MenuTree> lastAccepted() const;

private:
    const MenuSource &m_source;
    std::optional<Protocol::MenuTree> m_lastAccepted;
};

} // namespace QindaQt::Shell::GlobalMenu::Exporter
