// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/status_notifier/status_notifier_registry.h>

#include <qindaqt/shell/status_notifier/status_notifier_limits.h>

#include <QtGlobal>

#include <limits>

namespace QindaQt::StatusNotifier
{
namespace
{

constexpr quint64 kMaxGeneration = std::numeric_limits<quint64>::max();

} // namespace

quint64 StatusNotifierRegistry::beginOwnerGeneration(const QString &uniqueName)
{
    if (!isValidUniqueBusName(uniqueName)) {
        return 0;
    }

    const bool rebase = isOwnerLive(uniqueName);
    if (!rebase && m_generations.size() >= kMaxTrackedOwners) {
        // The owner table is bounded; a new name beyond capacity fails
        // closed instead of evicting a live owner or growing shell memory.
        return 0;
    }

    // AGENT-GUARD: The seed is the only source of generations and must never
    // wrap. A wrapped or reissued generation could match a stale event still
    // in flight and resurrect removed items; refusing the begin is safe.
    if (m_generationSeed == kMaxGeneration) {
        return 0;
    }
    const quint64 next = ++m_generationSeed;

    if (rebase) {
        // Watcher rebaseline of a still-live name: items of the previous
        // generation are dropped with their identity claims, so no stale or
        // unactionable key can remain presented or block re-registration.
        dropOwnerItems(uniqueName);
    }
    m_generations.insert(uniqueName, next);
    return next;
}

RegistryOutcome StatusNotifierRegistry::ownerLost(const QString &uniqueName,
                                                  quint64 expectedGeneration)
{
    if (!isValidUniqueBusName(uniqueName)) {
        return reject(RegistryStatus::InvalidOwner, QStringLiteral("owner-not-unique-bus-name"));
    }
    if (!isOwnerLive(uniqueName) || expectedGeneration != currentGeneration(uniqueName)) {
        // A loss that names an unknown owner, a departed owner, or an old
        // generation must not remove a later generation's items.
        return reject(RegistryStatus::StaleOwner, QStringLiteral("generation-not-current"));
    }

    dropOwnerItems(uniqueName);
    // The tracking slot is freed immediately: the globally unique generation
    // already fences any late event, so no departed-name residue is needed.
    m_generations.remove(uniqueName);
    return RegistryOutcome{RegistryStatus::Accepted, {}};
}

RegistryOutcome StatusNotifierRegistry::registerItem(const OwnerKey &key,
                                                     const ItemDescriptor &descriptor)
{
    const ValidationOutcome ownerOutcome = validateOwnerKey(key);
    if (!ownerOutcome.accepted) {
        return reject(RegistryStatus::InvalidOwner, ownerOutcome.reasonCode);
    }
    if (!isLiveGeneration(key)) {
        return reject(RegistryStatus::StaleOwner, QStringLiteral("generation-not-current"));
    }

    const ValidationOutcome descriptorOutcome = validateItemDescriptor(descriptor);
    if (!descriptorOutcome.accepted) {
        // A malformed replacement of a live item degrades the tray but keeps
        // the last-known-good descriptor presented.
        if (m_items.contains(key)) {
            m_degradedReason = QStringLiteral("malformed-item-replacement");
        }
        return reject(RegistryStatus::InvalidDescriptor, descriptorOutcome.reasonCode);
    }

    const auto identityOwner = m_identityOwners.constFind(descriptor.identity);
    if (identityOwner != m_identityOwners.cend() && identityOwner.value() != key) {
        return reject(RegistryStatus::DuplicateIdentity, QStringLiteral("identity-claimed"));
    }

    if (!m_items.contains(key) && m_items.size() >= kMaxItems) {
        m_degradedReason = QStringLiteral("item-capacity-exceeded");
        return reject(RegistryStatus::CapacityExceeded, QStringLiteral("item-capacity-exceeded"));
    }

    const auto existing = m_items.constFind(key);
    if (existing != m_items.cend() && existing->identity != descriptor.identity) {
        m_identityOwners.remove(existing->identity);
    }
    m_items.insert(key, descriptor);
    m_identityOwners.insert(descriptor.identity, key);
    return RegistryOutcome{RegistryStatus::Accepted, {}};
}

RegistryOutcome StatusNotifierRegistry::removeItem(const OwnerKey &key)
{
    const ValidationOutcome ownerOutcome = validateOwnerKey(key);
    if (!ownerOutcome.accepted) {
        return reject(RegistryStatus::InvalidOwner, ownerOutcome.reasonCode);
    }
    if (!isLiveGeneration(key)) {
        return reject(RegistryStatus::StaleOwner, QStringLiteral("generation-not-current"));
    }
    if (!m_items.contains(key)) {
        return reject(RegistryStatus::UnknownItem, QStringLiteral("item-not-registered"));
    }
    forgetItem(key);
    return RegistryOutcome{RegistryStatus::Accepted, {}};
}

RegistryOutcome StatusNotifierRegistry::removeAllForOwner(const QString &uniqueName,
                                                          quint64 generation)
{
    if (!isValidUniqueBusName(uniqueName)) {
        return reject(RegistryStatus::InvalidOwner, QStringLiteral("owner-not-unique-bus-name"));
    }
    if (generation == 0 || !isOwnerLive(uniqueName)
        || generation != currentGeneration(uniqueName)) {
        return reject(RegistryStatus::StaleOwner, QStringLiteral("generation-not-current"));
    }

    dropOwnerItems(uniqueName);
    return RegistryOutcome{RegistryStatus::Accepted, {}};
}

StatusNotifierRegistry::RequestEvaluation StatusNotifierRegistry::evaluateRequest(
    const OwnerKey &target, RequestKind kind) const
{
    RequestEvaluation evaluation;

    switch (kind) {
    case RequestKind::Activate:
    case RequestKind::ContextMenu:
    case RequestKind::SecondaryActivate:
        break;
    default:
        evaluation.outcome = reject(RegistryStatus::InvalidRequest,
                                    QStringLiteral("unknown-request-kind"));
        return evaluation;
    }

    const ValidationOutcome ownerOutcome = validateOwnerKey(target);
    if (!ownerOutcome.accepted) {
        evaluation.outcome = reject(RegistryStatus::InvalidOwner, ownerOutcome.reasonCode);
        return evaluation;
    }
    if (!isLiveGeneration(target)) {
        evaluation.outcome = reject(RegistryStatus::StaleOwner,
                                    QStringLiteral("generation-not-current"));
        return evaluation;
    }
    const auto item = m_items.constFind(target);
    if (item == m_items.cend()) {
        evaluation.outcome = reject(RegistryStatus::UnknownItem,
                                    QStringLiteral("item-not-registered"));
        return evaluation;
    }

    evaluation.outcome = RegistryOutcome{RegistryStatus::Accepted, {}};
    evaluation.intent.target = target;
    evaluation.intent.identity = item->identity;
    evaluation.intent.kind = kind;
    return evaluation;
}

void StatusNotifierRegistry::markInitialPopulationComplete()
{
    m_initialPopulationComplete = true;
}

void StatusNotifierRegistry::beginWatcherEpoch()
{
    // AGENT-GUARD: A watcher (re)connection must reset the population bit so
    // presentation falls back to fail-closed Loading; keeping the old bit
    // would present a pre-reconnect view as complete.
    m_initialPopulationComplete = false;
}

bool StatusNotifierRegistry::initialPopulationComplete() const noexcept
{
    return m_initialPopulationComplete;
}

bool StatusNotifierRegistry::isDegraded() const noexcept
{
    return !m_degradedReason.isEmpty();
}

QString StatusNotifierRegistry::degradedReason() const
{
    return m_degradedReason;
}

void StatusNotifierRegistry::acknowledgeDegraded()
{
    m_degradedReason.clear();
}

QList<OwnerKey> StatusNotifierRegistry::itemKeys() const
{
    return m_items.keys();
}

QList<ItemDescriptor> StatusNotifierRegistry::items() const
{
    return m_items.values();
}

std::optional<ItemDescriptor> StatusNotifierRegistry::find(const OwnerKey &key) const
{
    const auto iterator = m_items.constFind(key);
    if (iterator == m_items.cend()) {
        return std::nullopt;
    }
    return iterator.value();
}

bool StatusNotifierRegistry::contains(const OwnerKey &key) const
{
    return m_items.contains(key);
}

qsizetype StatusNotifierRegistry::count() const noexcept
{
    return m_items.size();
}

quint64 StatusNotifierRegistry::currentGeneration(const QString &uniqueName) const
{
    return m_generations.value(uniqueName, 0);
}

bool StatusNotifierRegistry::isOwnerLive(const QString &uniqueName) const
{
    return m_generations.contains(uniqueName);
}

RegistryOutcome StatusNotifierRegistry::reject(RegistryStatus status, QString reasonCode) const
{
    return RegistryOutcome{status, std::move(reasonCode)};
}

void StatusNotifierRegistry::forgetItem(const OwnerKey &key)
{
    const auto iterator = m_items.constFind(key);
    if (iterator == m_items.cend()) {
        return;
    }
    // The identity index points at this exact key only while this item owns
    // the identity; identity reuse across owners never overwrites it.
    if (m_identityOwners.value(iterator->identity) == key) {
        m_identityOwners.remove(iterator->identity);
    }
    m_items.erase(iterator);
}

void StatusNotifierRegistry::dropOwnerItems(const QString &uniqueName)
{
    for (auto iterator = m_items.begin(); iterator != m_items.end();) {
        if (iterator.key().uniqueName == uniqueName) {
            if (m_identityOwners.value(iterator->identity) == iterator.key()) {
                m_identityOwners.remove(iterator->identity);
            }
            iterator = m_items.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

bool StatusNotifierRegistry::isLiveGeneration(const OwnerKey &key) const
{
    // Liveness is as important as the generation match: only live owners are
    // tracked, so a retired or never-seen name always fails here, and the
    // globally unique seed guarantees a departed owner's generation can
    // never be reissued to a future owner.
    return key.generation != 0 && isOwnerLive(key.uniqueName)
        && key.generation == currentGeneration(key.uniqueName);
}

} // namespace QindaQt::StatusNotifier
