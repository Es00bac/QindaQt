// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_customization/editing_commands.h"
#include "qindaqt/shell_customization/layout_editing_repository.h"
#include "qindaqt/shell_customization_editor/editing_engine.h"

#include <QThread>
#include <QVector>

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
    CoordinatorEditingEngine(
        QindaQt::ShellCustomization::LayoutEditingRepository &repository,
        QVector<QindaQt::Applets::AppletManifest> manifestCatalog = {});
    ~CoordinatorEditingEngine() override;

    CoordinatorEditingEngine(const CoordinatorEditingEngine &) = delete;
    CoordinatorEditingEngine &operator=(const CoordinatorEditingEngine &) = delete;

    [[nodiscard]] QindaQt::ShellCustomization::EditingResult
    execute(const QindaQt::ShellCustomization::EditingCommand &command) override;
    [[nodiscard]] QindaQt::ShellCustomization::EditingEvaluation
    evaluate(const QindaQt::ShellCustomization::EditingCommand &command) const override;
    [[nodiscard]] SequenceEvaluation evaluateSequence(
        const QVector<QindaQt::ShellCustomization::EditingCommand> &commands) const override;
    [[nodiscard]] std::shared_ptr<const QindaQt::ShellCustomization::LayoutEditingSnapshot>
    snapshot() const override;
    [[nodiscard]] bool isReady() const override;
    [[nodiscard]] std::shared_ptr<const QindaQt::Profiles::LayoutProfile>
    committedProfile() const override;
    [[nodiscard]] QindaQt::ShellCustomization::LayoutEditingStatus status() const override;
    [[nodiscard]] bool hasPreview() const override;

    // Compatibility spelling for presentation consumers. This is exactly the
    // EditingEngine readiness contract, not a separate lease probe.
    [[nodiscard]] bool holdsLease() const;

private:
    [[nodiscard]] bool onOwnerThread() const noexcept;
    [[nodiscard]] bool ensureCoordinator() const;

    QindaQt::ShellCustomization::LayoutEditingRepository &m_repository;
    QVector<QindaQt::Applets::AppletManifest> m_manifestCatalog;
    QThread *m_ownerThread = nullptr;
    mutable std::unique_ptr<QindaQt::ShellCustomization::LayoutEditingCoordinator> m_coordinator;
};

} // namespace QindaQt::ShellCustomizationEditor
