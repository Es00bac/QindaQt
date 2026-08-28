// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/status_notifier/status_notifier_types.h>
#include <qindaqt/shell/status_notifier/status_notifier_validation.h>

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QString>

#include <optional>

namespace QindaQt::StatusNotifier
{

enum class RegistryStatus : quint32 {
    Accepted = 0,
    InvalidOwner = 1,
    InvalidDescriptor = 2,
    UnknownItem = 3,
    StaleOwner = 4,
    DuplicateIdentity = 5,
    CapacityExceeded = 6,
    InvalidRequest = 7,
};

struct RegistryOutcome {
    RegistryStatus status = RegistryStatus::Accepted;
    QString reasonCode;

    [[nodiscard]] bool accepted() const noexcept { return status == RegistryStatus::Accepted; }

    friend bool operator==(const RegistryOutcome &, const RegistryOutcome &) = default;
};

// AGENT-CONTRACT: Exact-owner keyed tray item registry.
//
// The transport side (an injected adapter; never a live bus inside this
// module) calls beginOwnerGeneration() when a unique name appears, feeds
// keyed events, and calls ownerLost() when the name departs. Every event
// carries an OwnerKey whose generation must equal the owner's current
// generation, so a reply that races a disconnect or restart is rejected as
// stale instead of resurrecting removed items. Ownership lives on the bus
// unique name; a well-known name is rejected as an owner.
class StatusNotifierRegistry
{
public:
    StatusNotifierRegistry() = default;

    // Allocates the next generation for `uniqueName` (first sight is
    // generation 1). Returns 0 when the name cannot be an owner.
    [[nodiscard]] quint32 beginOwnerGeneration(const QString &uniqueName);

    // Drops every item of `uniqueName` and invalidates its current generation.
    // Later events stamped with any previously allocated generation are stale.
    void ownerLost(const QString &uniqueName);

    // Registers or replaces the item at `key`. Replacing the same key of a
    // live owner with a valid descriptor is the supported update path; a
    // malformed replacement is rejected and degrades the registry while the
    // last-known-good item stays presented.
    [[nodiscard]] RegistryOutcome registerItem(const OwnerKey &key, const ItemDescriptor &descriptor);
    [[nodiscard]] RegistryOutcome removeItem(const OwnerKey &key);

    // Removes every item of a still-live owner in one event, e.g. when the
    // source reports all its items unregistered. Generation-fenced like any
    // other keyed event.
    [[nodiscard]] RegistryOutcome removeAllForOwner(const QString &uniqueName, quint32 generation);

    // Validates a user-visible request intent against exact live ownership.
    // This never performs the request; it only answers whether the intent
    // targets an item that the named owner currently presents.
    [[nodiscard]] RegistryOutcome evaluateRequest(const OwnerKey &target, RequestKind kind) const;

    // Marks the initial population phase complete (transport observed the
    // watcher's registered-items state once). Presentation may show Empty
    // only after this.
    void markInitialPopulationComplete();
    [[nodiscard]] bool initialPopulationComplete() const noexcept;

    [[nodiscard]] bool isDegraded() const noexcept;
    [[nodiscard]] QString degradedReason() const;
    void acknowledgeDegraded();

    [[nodiscard]] QList<OwnerKey> itemKeys() const;
    [[nodiscard]] QList<ItemDescriptor> items() const;
    [[nodiscard]] std::optional<ItemDescriptor> find(const OwnerKey &key) const;
    [[nodiscard]] bool contains(const OwnerKey &key) const;
    [[nodiscard]] qsizetype count() const noexcept;
    [[nodiscard]] quint32 currentGeneration(const QString &uniqueName) const;
    [[nodiscard]] bool isOwnerLive(const QString &uniqueName) const;

private:
    [[nodiscard]] RegistryOutcome reject(RegistryStatus status, QString reasonCode) const;
    void forgetItem(const OwnerKey &key);

    QHash<QString, quint32> m_generations;
    // A departed owner keeps its last allocated generation so stale events
    // remain fenced; beginOwnerGeneration() always advances beyond it.
    QHash<QString, bool> m_ownerLive;
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
