// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_visibility_protocol/wire_limits.h"

#include <QByteArray>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QVector>

namespace QindaQt::Compositor {

using ShellVisibilityWireLimits = ShellVisibilityProtocol::WireLimits;

struct ShellVisibilityOutputSnapshot final
{
    QString id;
    QRect geometry;
    qreal scale = 1.0;
};

struct ShellVisibilityWindowSnapshot final
{
    QString id;
    QString outputId;
    QRect frameGeometry;
    QStringList workspaceIds;
    QStringList activityIds;
    bool onAllWorkspaces = false;
    bool active = false;
    bool maximized = false;
    bool minimized = false;
    bool hidden = false;
};

struct ShellVisibilityScopeSnapshot final
{
    QString workspaceId;
    QString activityId;
};

struct ShellVisibilitySnapshotCandidate final
{
    ShellVisibilityScopeSnapshot scope;
    QVector<ShellVisibilityOutputSnapshot> outputs;
    QVector<ShellVisibilityWindowSnapshot> windows;
    // Decimal-string generation of the exact Outputs projection used above.
    // Kept last so existing aggregate producers retain source compatibility.
    quint64 outputGeneration = 1;
};

enum class ShellVisibilityPublishResult {
    Published,
    Unchanged,
    Rejected,
    RevisionExhausted,
};

struct ShellVisibilityRevisionSeed final
{
    quint64 value = 0;
};

// Retains one canonical, immutable wire generation. The store has no timers,
// KWin objects, or D-Bus side effects; its owner serializes all calls on one
// thread and decides when a Published result becomes an IPC invalidation.
class ShellVisibilitySnapshotStore final
{
public:
    explicit ShellVisibilitySnapshotStore(
        QString epoch,
        ShellVisibilityRevisionSeed revisionSeed = {});

    [[nodiscard]] ShellVisibilityPublishResult publish(
        const ShellVisibilitySnapshotCandidate &candidate,
        QString *error = nullptr);
    [[nodiscard]] const QByteArray &snapshotJson() const noexcept;
    [[nodiscard]] quint64 revision() const noexcept;
    [[nodiscard]] const QString &epoch() const noexcept;
    [[nodiscard]] bool available() const noexcept;
    // Returns true only for the available-to-unavailable edge. The caller emits
    // one invalidation for that edge and stays quiet for repeated failures.
    [[nodiscard]] bool markUnavailable(const QString &code,
                                       const QString &message);

private:
    QByteArray m_snapshotJson;
    QByteArray m_canonicalState;
    QString m_epoch;
    quint64 m_revision = 0;
    bool m_available = false;
};

} // namespace QindaQt::Compositor
