// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/applets/applet_manifest.h"
#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_customization/editing_result.h"
#include "qindaqt/shell_layout/panel_layout_types.h"

#include <QVector>
#include <QtTypes>

#include <memory>

namespace QindaQt::ShellCustomization {

// A panel the stored profile pins to an output this generation does not have.
//
// The editor cannot place such a panel and must not pretend to: it has nowhere
// to be until its display returns. It must equally never be *lost*, because
// the stored profile is the only record that the user ever configured it.
// Escrow is that middle state - carried, not edited, not laid out - and it is
// re-attached at the persistence boundary so Apply writes the panel back
// unchanged.
struct EscrowedPanel final {
    // The index the panel held in the stored profile. A merge restores stored
    // order instead of appending, so a round trip through the editor does not
    // reorder panels behind the user's back.
    qsizetype index = 0;
    Profiles::PanelSpec panel;
};

struct LayoutEditingSnapshot final {
    // Profiles are loader-normalized and layout contains the corresponding
    // successful solve over every panel the generation can host. Panels pinned
    // to an absent output are not here; see EscrowedPanel. Neither value is
    // exposed independently.
    Profiles::LayoutProfile profile;
    ShellLayout::PanelLayoutResult layout;
    quint64 revision = 0;
    bool previewActive = false;
};

struct LayoutEditingStatus final {
    bool previewActive = false;
    bool previewDirty = false;
    bool canUndo = false;
    bool canRedo = false;

    bool operator==(const LayoutEditingStatus &) const = default;
};

class LayoutEditingCoordinator;

// The repository owns immutable snapshots and the logical output inventory.
// Retained shared pointers remain valid across later publications. The class
// is intentionally not thread-safe: one settings/editor thread must serialize
// all coordinator calls and snapshot reads. Invalid initial data produces a
// non-ready repository with no snapshot; initializationError() retains the
// cause, and coordinators reject every command until the repository is replaced.
class LayoutEditingRepository final {
public:
    // This compatibility overload captures an empty manifest catalog. It can
    // inspect, reorder, and remove legacy applets, but commands that create or
    // change a placement fail with ManifestUnavailable.
    LayoutEditingRepository(Profiles::LayoutProfile initialProfile,
                            QVector<ShellLayout::LogicalOutput> outputs,
                            quint64 initialRevision = 0);
    LayoutEditingRepository(Profiles::LayoutProfile initialProfile,
                            QVector<ShellLayout::LogicalOutput> outputs,
                            QVector<Applets::AppletManifest> manifestCatalog,
                            quint64 initialRevision = 0);
    ~LayoutEditingRepository();

    LayoutEditingRepository(const LayoutEditingRepository &) = delete;
    LayoutEditingRepository &operator=(const LayoutEditingRepository &) = delete;
    LayoutEditingRepository(LayoutEditingRepository &&) = delete;
    LayoutEditingRepository &operator=(LayoutEditingRepository &&) = delete;

    [[nodiscard]] bool isReady() const noexcept;
    [[nodiscard]] const EditingError &initializationError() const noexcept;
    // A null result means initialization failed. Every non-null snapshot obeys
    // LayoutEditingSnapshot's normalized-profile/successful-solve invariant.
    [[nodiscard]] std::shared_ptr<const LayoutEditingSnapshot> snapshot() const noexcept;
    // This value is a coherent read of repository-owned transaction state on
    // the editor thread. Mutating the returned copy cannot affect the session.
    [[nodiscard]] LayoutEditingStatus status() const noexcept;
    // The returned inventory is owned by the repository and valid until its
    // destruction. Output changes create a new editor session at this boundary.
    [[nodiscard]] const QVector<ShellLayout::LogicalOutput> &outputs() const noexcept;

    // Panels held out of this session because their output is absent, in
    // stored order. Computed once at construction and never by candidate
    // validation, which is what keeps an *edit* that names a disconnected
    // display refused while a *pre-existing* pin is merely parked.
    [[nodiscard]] const QVector<EscrowedPanel> &escrowedPanels() const noexcept;

    // Re-attaches escrowed panels to an edited profile at their stored
    // indices.
    // AGENT-CONTRACT: every write of an edited profile to durable storage must
    // go through this. Persisting a session profile directly erases the user's
    // panels on whichever displays happened to be absent during the edit.
    [[nodiscard]] static Profiles::LayoutProfile withEscrowedPanels(
        const Profiles::LayoutProfile &edited,
        const QVector<EscrowedPanel> &escrowed);

    // The unique pointer is the sole move-only editing lease. A null result
    // means another coordinator still owns the session. Destroying the lease
    // releases it without discarding repository-owned history or preview state.
    // AGENT-CONTRACT: The repository must outlive its returned lease.
    [[nodiscard]] std::unique_ptr<LayoutEditingCoordinator> tryAcquireCoordinator();

private:
    struct SessionState;

    void publish(std::shared_ptr<const LayoutEditingSnapshot> snapshot) noexcept;
    void releaseCoordinator() noexcept;

    QVector<ShellLayout::LogicalOutput> m_outputs;
    std::unique_ptr<SessionState> m_session;

    friend class LayoutEditingCoordinator;
};

} // namespace QindaQt::ShellCustomization
