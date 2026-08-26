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

struct LayoutEditingSnapshot final {
    // Profiles are loader-normalized and layout contains the corresponding
    // successful all-output solve. Neither value is exposed independently.
    Profiles::LayoutProfile profile;
    ShellLayout::PanelLayoutResult layout;
    quint64 revision = 0;
    bool previewActive = false;
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
    // The returned inventory is owned by the repository and valid until its
    // destruction. Output changes create a new editor session at this boundary.
    [[nodiscard]] const QVector<ShellLayout::LogicalOutput> &outputs() const noexcept;

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
