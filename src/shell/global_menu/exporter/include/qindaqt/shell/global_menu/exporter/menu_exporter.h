// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/exporter/menu_source.h>
#include <qindaqt/shell/global_menu/protocol/menu_tree.h>
#include <qindaqt/shell/global_menu/protocol/menu_validation.h>

#include <QtCore/QUuid>
#include <QtCore/QtGlobal>

#include <optional>

namespace QindaQt::Shell::GlobalMenu::Exporter
{

// The authoritative lineage for one owner window, minted by the single
// ownership authority (in shell composition, the ActiveProviderSelector) and
// stamped into every accepted tree. AGENT-CONTRACT: the exporter never
// mints lineage itself — without this binding there is no coherent lineage a
// consumer can check an invocation against. An owner without current
// authority yields std::nullopt and the pull fails closed.
struct ExportLineage final {
    QUuid epoch;
    quint64 revision = 0;

    bool operator==(const ExportLineage &) const = default;
};

class ExportLineageSource
{
public:
    virtual ~ExportLineageSource() = default;

    // Must be synchronous and side-effect-free. Implemented by shell
    // composition on top of the ownership selector; G0 tests provide fakes.
    [[nodiscard]] virtual std::optional<ExportLineage> lineageFor(
        const QUuid &ownerWindowId) const = 0;
};

enum class ExportOutcome {
    // The snapshot was valid and its content differs from the last accepted
    // tree under the same owner; it is now authoritative.
    Published,
    // The snapshot was valid and its content is identical to the last
    // accepted tree; nothing changed.
    Unchanged,
    // The snapshot failed canonical validation. The last accepted tree, if
    // any, is untouched: a malformed pull never regresses a previously good
    // menu, matching the applet-manifest catalog's atomic-load convention.
    RejectedInvalid,
    // The source reported an incomplete traversal (overflow, cycle, or
    // mid-traversal defect). The last accepted tree is untouched; a bounded
    // prefix is never published.
    RejectedIncomplete,
    // No current lineage exists for the snapshot's owner window, so there is
    // no authority to publish under. The last accepted tree is untouched.
    RejectedNoAuthority,
};

struct ExportResult final {
    ExportOutcome outcome = ExportOutcome::RejectedInvalid;
    Protocol::ValidationResult validation;
    // Meaningful for Published/Unchanged: whether the accepted snapshot's
    // content differs from the previously accepted content under the same
    // owner. The full-tree delta contract is deferred to the transport
    // milestone; snapshot truth is the only G0 guarantee.
    bool changed = false;
    // Empty unless the outcome is RejectedIncomplete.
    QString defectCode;
};

// Pulls, validates, and stamps an authoritative lineage onto a MenuSource's
// snapshots. The exporter owns nothing about lineage except stamping: epoch
// and revision come from the injected ExportLineageSource, so owner, epoch,
// revision, and the invocation guard's expectations share one source of
// truth.
//
// Lifetime/thread contract: both references must outlive this object, and
// refresh()/lastAccepted() must run on the thread that owns the source (the
// Qt GUI thread for widget-backed sources); calls are synchronous.
class MenuExporter final
{
public:
    MenuExporter(const MenuSource &source, const ExportLineageSource &lineageSource);

    [[nodiscard]] ExportResult refresh();
    [[nodiscard]] std::optional<Protocol::MenuTree> lastAccepted() const;

private:
    const MenuSource &m_source;
    const ExportLineageSource &m_lineageSource;
    std::optional<Protocol::MenuTree> m_lastAccepted;
};

} // namespace QindaQt::Shell::GlobalMenu::Exporter
