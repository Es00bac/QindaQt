// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_visibility/panel_visibility_types.h"

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>
#include <QtTypes>

#include <optional>

namespace QindaQt::ShellOrchestration {

class PanelInteractionStore;

enum class PanelInteractionKind {
    Reveal,
    VisibilityHold,
};

// A move-only source lease. Independent menus, pointer regions, shortcuts, and
// animations may hold the same intent without prematurely clearing each other.
class PanelInteractionLease final {
public:
    PanelInteractionLease() = default;
    PanelInteractionLease(const PanelInteractionLease &) = delete;
    PanelInteractionLease &operator=(const PanelInteractionLease &) = delete;
    PanelInteractionLease(PanelInteractionLease &&other) noexcept;
    PanelInteractionLease &operator=(PanelInteractionLease &&other) noexcept;
    ~PanelInteractionLease();

    [[nodiscard]] bool valid() const noexcept;
    void reset();

private:
    friend class PanelInteractionStore;
    PanelInteractionLease(PanelInteractionStore *store, quint64 token);

    QPointer<PanelInteractionStore> m_store;
    quint64 m_token = 0;
};

class PanelInteractionStore final : public QObject {
    Q_OBJECT

public:
    explicit PanelInteractionStore(QObject *parent = nullptr);
    ~PanelInteractionStore() override;

    // Replaces the complete valid panel identity set. Leases for removed
    // identities are invalidated atomically and later destruction is a no-op.
    [[nodiscard]] bool setIdentities(
        const QVector<ShellVisibility::PanelSurfaceIdentity> &identities,
        QString *error = nullptr);
    [[nodiscard]] std::optional<PanelInteractionLease> acquire(
        const ShellVisibility::PanelSurfaceIdentity &identity,
        PanelInteractionKind kind,
        QString *error = nullptr);

    [[nodiscard]] QVector<ShellVisibility::PanelInteractionSnapshot>
    snapshot() const;

Q_SIGNALS:
    void interactionsChanged();

private:
    friend class PanelInteractionLease;
    [[nodiscard]] bool containsToken(quint64 token) const noexcept;
    void release(quint64 token);

    class Private;
    Private *m_private = nullptr;
};

} // namespace QindaQt::ShellOrchestration
