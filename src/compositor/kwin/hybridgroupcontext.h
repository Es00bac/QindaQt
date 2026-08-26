// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QMap>
#include <QRect>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

namespace QindaQt::Compositor::KWinIntegration {

// Value-only container context shared by the scene transaction and the KWin
// signal adapter. It deliberately excludes geometry and focus: those remain
// scene-owned so a context change cannot bypass constraint solving or rollback.
struct HybridGroupContext final
{
    QString outputId;
    QStringList desktopIds;
    QStringList activityIds;
    bool keepAbove = false;
    bool keepBelow = false;

    [[nodiscard]] bool isValid(QString *error = nullptr) const;

    friend bool operator==(const HybridGroupContext &,
                           const HybridGroupContext &) = default;
};

enum class HybridGroupContextRecoveryStatus {
    Adopted,
    Released,
    Quarantined,
};

struct HybridGroupContextRecoveryResult final
{
    HybridGroupContextRecoveryStatus status =
        HybridGroupContextRecoveryStatus::Quarantined;
    QString adoptionError;
    QString releaseError;
};

using HybridGroupContextOperation = std::function<bool(QString *error)>;
using HybridGroupContextSafetyUpdate =
    std::function<void(const QString &containerId, bool coherent)>;

// Runs synchronously and retains no callback. A failed adoption first attempts
// normal container release; only a double failure marks the remaining group
// unsafe. A later successful adoption is the sole explicit coherence proof.
[[nodiscard]] HybridGroupContextRecoveryResult recoverHybridGroupContext(
    const QString &containerId,
    const HybridGroupContextOperation &adopt,
    const HybridGroupContextOperation &release,
    const HybridGroupContextSafetyUpdate &updateSafety);

// Tracks the containers whose live members may disagree on their atomic group
// context. It owns stable IDs only and is confined to the compositor thread.
class HybridGroupContextQuarantine final
{
public:
    void quarantine(const QString &containerId)
    {
        if (!containerId.isEmpty()) {
            m_containerIds.insert(containerId);
        }
    }
    void markCoherent(const QString &containerId)
    {
        m_containerIds.remove(containerId);
    }
    void reconcilePublishedContainers(const QStringList &containerIds)
    {
        for (auto iterator = m_containerIds.begin();
             iterator != m_containerIds.end();) {
            if (containerIds.contains(*iterator)) {
                ++iterator;
            } else {
                iterator = m_containerIds.erase(iterator);
            }
        }
    }
    [[nodiscard]] bool contains(const QString &containerId) const noexcept
    {
        return m_containerIds.contains(containerId);
    }
    [[nodiscard]] qsizetype size() const noexcept
    {
        return m_containerIds.size();
    }

private:
    // AGENT-GUARD: Ordinary chrome reconciliation must never clear an ID that
    // is still present. Only disappearance from a successfully published
    // topology or an explicit successful context adoption proves it safe.
    QSet<QString> m_containerIds;
};

// Maps the complete outer frame with KWin's relative-to-output-center rule.
// Integer rounding happens once at this boundary before constraint solving.
[[nodiscard]] std::optional<QRect> mapHybridGroupFrameToArea(
    const QRect &frame,
    const QRectF &oldArea,
    const QRectF &newArea,
    QString *error = nullptr);

// Preserves queued signal work across an unrelated chrome resubscription while
// dropping sources whose topology or live registry ownership changed.
[[nodiscard]] QMap<QString, QString> retainValidGroupContextSources(
    const QMap<QString, QString> &pendingSourceByContainer,
    const QHash<QString, QString> &containerForWindow,
    const QHash<QString, QString> &liveOwnerForWindow);

} // namespace QindaQt::Compositor::KWinIntegration
