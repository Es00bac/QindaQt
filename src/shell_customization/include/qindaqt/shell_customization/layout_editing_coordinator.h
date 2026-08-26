// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_customization/editing_commands.h"
#include "qindaqt/shell_customization/editing_result.h"
#include "qindaqt/shell_customization/layout_editing_repository.h"

namespace QindaQt::ShellCustomization {

// The coordinator is acquired only through LayoutEditingRepository. The
// returned unique_ptr is the move-only lease; concurrent acquisition fails.
// Execution is synchronous, non-reentrant, and confined to the repository's
// owner thread. A command that returns an EditingResult failure leaves snapshot,
// revision, preview, and repository-owned history intact.
class LayoutEditingCoordinator final {
public:
    ~LayoutEditingCoordinator();

    LayoutEditingCoordinator(const LayoutEditingCoordinator &) = delete;
    LayoutEditingCoordinator &operator=(const LayoutEditingCoordinator &) = delete;
    LayoutEditingCoordinator(LayoutEditingCoordinator &&) = delete;
    LayoutEditingCoordinator &operator=(LayoutEditingCoordinator &&) = delete;

    [[nodiscard]] EditingResult execute(const EditingCommand &command);
    // Runs the same preflight, mutation, placement, profile-round-trip, and
    // all-output layout checks as execute(), but publishes no snapshot and
    // changes no revision or history. Acceptance remains valid only while the
    // repository stays at EditingEvaluation::revision.
    [[nodiscard]] EditingEvaluation evaluate(const EditingCommand &command) const;
    // The retained pointer survives later publications and lease handoff.
    // Preview-only edits deliberately do not change it. A null result means
    // the repository never initialized and therefore has no committed value.
    [[nodiscard]] std::shared_ptr<const Profiles::LayoutProfile>
    committedProfile() const noexcept;
    [[nodiscard]] bool hasPreview() const noexcept;

private:
    explicit LayoutEditingCoordinator(LayoutEditingRepository &repository);

    [[nodiscard]] EditingResult executeEdit(const EditingCommand &command,
                                            EditingCommandKind kind,
                                            quint64 beforeRevision);
    [[nodiscard]] EditingResult beginPreview(EditingCommandKind kind, quint64 beforeRevision);
    [[nodiscard]] EditingResult commitPreview(EditingCommandKind kind, quint64 beforeRevision);
    [[nodiscard]] EditingResult cancelPreview(EditingCommandKind kind, quint64 beforeRevision);
    [[nodiscard]] EditingResult undo(EditingCommandKind kind, quint64 beforeRevision);
    [[nodiscard]] EditingResult redo(EditingCommandKind kind, quint64 beforeRevision);

    LayoutEditingRepository &m_repository;

    friend class LayoutEditingRepository;
};

} // namespace QindaQt::ShellCustomization
