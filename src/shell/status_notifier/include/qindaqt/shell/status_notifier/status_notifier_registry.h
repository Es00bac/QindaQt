// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/status_notifier/status_notifier_event_sink.h>
#include <qindaqt/shell/status_notifier/status_notifier_types.h>
#include <qindaqt/shell/status_notifier/status_notifier_validation.h>

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QString>

#include <optional>

namespace QindaQt::StatusNotifier
{

// AGENT-CONTRACT: Exact-owner keyed tray item registry, and the only
// StatusNotifierEventSink implementation. Transport adapters reach it solely
// through the narrow sink interface; observation, request evaluation, and
// degradation acknowledgement exist only on this concrete class.
//
// Fencing model: generations come from one globally monotonic counter, so a
// generation value is never reissued — not after owner loss, bounded-history
// eviction, or counter-wrap refusal. Only live owners occupy tracking slots;
// an ownerLost() retires the name immediately and drops its items. Every
// keyed event (registration, removal, mass removal, loss) must carry the
// owner's current generation, so a reply that races a disconnect or restart
// is rejected as stale instead of resurrecting removed items. Ownership lives
// on the bus unique name; a well-known name is rejected as an owner.
class StatusNotifierRegistry final : public StatusNotifierEventSink
{
public:
    // A typed accepted request: `intent` is meaningful only when
    // `outcome.accepted()`. The intent binds the exact owner key (including
    // its current generation) and the item identity snapshot at acceptance
    // time; an executor must revalidate before performing anything.
    struct RequestEvaluation {
        RegistryOutcome outcome;
        RequestIntent intent;

        friend bool operator==(const RequestEvaluation &,
                               const RequestEvaluation &) = default;
    };

    StatusNotifierRegistry() = default;

    // AGENT-NOTE: Re-basing a still-live name is the watcher-rebaseline
    // path: the owner's items are dropped deterministically and a fresh
    // generation is issued, so no presented key can survive unactionable.
    [[nodiscard]] quint64 beginOwnerGeneration(const QString &uniqueName) override;
    [[nodiscard]] RegistryOutcome ownerLost(const QString &uniqueName,
                                            quint64 expectedGeneration) override;

    // Registers or replaces the item at `key`. Replacing the same key of a
    // live owner with a valid descriptor is the supported update path; a
    // malformed replacement is rejected and degrades the registry while the
    // last-known-good item stays presented.
    [[nodiscard]] RegistryOutcome registerItem(const OwnerKey &key,
                                               const ItemDescriptor &descriptor) override;
    [[nodiscard]] RegistryOutcome removeItem(const OwnerKey &key) override;
    [[nodiscard]] RegistryOutcome removeAllForOwner(const QString &uniqueName,
                                                    quint64 generation) override;

    void markInitialPopulationComplete() override;
    void beginWatcherEpoch() override;

    // Validates a user-visible request intent against exact live ownership
    // and returns it bound to the current owner generation and item identity.
    // This never performs the request; see RequestIntent for the lifetime the
    // executor must honor.
    [[nodiscard]] RequestEvaluation evaluateRequest(const OwnerKey &target,
                                                    RequestKind kind) const;

    [[nodiscard]] bool initialPopulationComplete() const noexcept;

    [[nodiscard]] bool isDegraded() const noexcept;
    [[nodiscard]] QString degradedReason() const;
    void acknowledgeDegraded();

    [[nodiscard]] QList<OwnerKey> itemKeys() const;
    [[nodiscard]] QList<ItemDescriptor> items() const;
    [[nodiscard]] std::optional<ItemDescriptor> find(const OwnerKey &key) const;
    [[nodiscard]] bool contains(const OwnerKey &key) const;
    [[nodiscard]] qsizetype count() const noexcept;
    [[nodiscard]] quint64 currentGeneration(const QString &uniqueName) const;
    [[nodiscard]] bool isOwnerLive(const QString &uniqueName) const;

private:
    [[nodiscard]] RegistryOutcome reject(RegistryStatus status, QString reasonCode) const;
    void forgetItem(const OwnerKey &key);
    // Drops every item of the owner and frees its identity claims.
    void dropOwnerItems(const QString &uniqueName);
    [[nodiscard]] bool isLiveGeneration(const OwnerKey &key) const;

    // AGENT-GUARD: Only live owners appear in m_generations, and the table is
    // capped at kMaxTrackedOwners; nothing else retains per-owner state, so
    // owner churn cannot grow registry memory. Generations are drawn from
    // m_generationSeed, which is globally monotonic — it is never reset, and
    // exhaustion fails closed instead of wrapping to a stale value.
    QHash<QString, quint64> m_generations;
    quint64 m_generationSeed = 0;
    QHash<OwnerKey, ItemDescriptor> m_items;
    // AGENT-GUARD: Reverse index identity -> owning key. It must stay exactly
    // in sync with m_items; every m_items mutation updates both or neither.
    QHash<QString, OwnerKey> m_identityOwners;
    bool m_initialPopulationComplete = false;
    QString m_degradedReason;
};

// OwnerKey is hashable so the registry can key items by exact owner. The
// helper is used unqualified: a locally declared qHash would otherwise shadow
// Qt's fundamental-type overloads for the generation field.
[[nodiscard]] inline size_t qHash(const OwnerKey &key, size_t seed = 0) noexcept
{
    return qHashMulti(seed, key.uniqueName, key.objectPath, key.generation);
}

} // namespace QindaQt::StatusNotifier
