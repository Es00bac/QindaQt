// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_customization/editing_commands.h"
#include "qindaqt/shell_customization/layout_editing_repository.h"
#include "qindaqt/shell_customization_editor/editing_engine.h"

#include <memory>

namespace QindaQt::ShellCustomizationEditor {

// Production EditingEngine: wraps the move-only coordinator lease acquired
// from one repository. While no lease is held (another coordinator owns the
// session, or the repository never initialized) every call returns a typed
// failure and the editor presents read-only, per the collision rule in the
// customization architecture.
class CoordinatorEditingEngine final : public EditingEngine {
public:
    // AGENT-CONTRACT: the repository must outlive this adapter, mirroring the
    // repository's own lease contract. Construction attempts the lease; a
    // null lease degrades to typed EngineUnavailable results instead of
    // throwing, so a losing editor stays presentable.
    explicit CoordinatorEditingEngine(QindaQt::ShellCustomization::LayoutEditingRepository &repository);
    ~CoordinatorEditingEngine() override;

    CoordinatorEditingEngine(const CoordinatorEditingEngine &) = delete;
    CoordinatorEditingEngine &operator=(const CoordinatorEditingEngine &) = delete;

    [[nodiscard]] QindaQt::ShellCustomization::EditingResult
    execute(const QindaQt::ShellCustomization::EditingCommand &command) override;
    [[nodiscard]] QindaQt::ShellCustomization::EditingEvaluation
    evaluate(const QindaQt::ShellCustomization::EditingCommand &command) const override;
    [[nodiscard]] std::shared_ptr<const QindaQt::ShellCustomization::LayoutEditingSnapshot>
    snapshot() const override;
    [[nodiscard]] bool hasPreview() const override;

    // False while another coordinator owns the session or the repository is
    // not ready. The presentation binds its read-only banner to this value.
    [[nodiscard]] bool holdsLease() const noexcept;

private:
    QindaQt::ShellCustomization::LayoutEditingRepository &m_repository;
    std::unique_ptr<QindaQt::ShellCustomization::LayoutEditingCoordinator> m_coordinator;
};

} // namespace QindaQt::ShellCustomizationEditor
