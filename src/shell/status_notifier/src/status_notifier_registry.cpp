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

StatusNotifierRegistry::StatusNotifierRegistry(quint64 initialGenerationSeed,
                                               quint64 initialWatcherEpochSeed)
    : m_generationSeed(initialGenerationSeed)
    , m_watcherEpochSeed(initialWatcherEpochSeed)
{
}

quint64 StatusNotifierRegistry::beginWatcherEpoch()
{
    // AGENT-GUARD: Watcher epochs are monotonic. When a new watcher arrives,
    // the population bit resets to fail-closed Loading until the replacement
    // watcher's population is observed and marked complete.
    if (m_watcherEpochSeed == kMaxGeneration) {
        // There is no safe epoch value left for the replacement watcher. Drop
        // the active epoch as well as the population bit so traffic stamped by
        // the previous watcher cannot mutate state after this failed handoff.
        m_currentWatcherEpoch = 0;
        m_initialPopulationComplete = false;
        m_reconcilingPopulation = false;
        m_stagedItems.clear();
        m_stagedIdentityOwners.clear();
        m_stagedPopulationFailure = {};
        return 0;
    }
    m_currentWatcherEpoch = ++m_watcherEpochSeed;
    m_initialPopulationComplete = false;
    // The first population has no accepted LKG snapshot to protect, so its
    // events retain the established immediate registry semantics. Every later
    // watcher stages its complete target beside the published LKG snapshot.
    m_reconcilingPopulation = m_hasCompletedPopulation;
    m_stagedItems.clear();
    m_stagedIdentityOwners.clear();
    m_stagedPopulationFailure = {};
    return m_currentWatcherEpoch;
}

RegistryOutcome StatusNotifierRegistry::markInitialPopulationComplete(quint64 epoch)
{
    if (epoch == 0 || epoch != m_currentWatcherEpoch) {
        return reject(RegistryStatus::StaleOwner, QStringLiteral("stale-watcher-epoch"));
    }

    if (m_reconcilingPopulation) {
        if (!m_stagedPopulationFailure.accepted()) {
            return m_stagedPopulationFailure;
        }

        // AGENT-GUARD: Publish the complete target and both indexes together.
        // Mutating m_items incrementally would expose duplicate identity claims
        // and make valid 64-for-64 replacement depend on event order.
        m_items = std::move(m_stagedItems);
        m_identityOwners = std::move(m_stagedIdentityOwners);
        m_itemLastSeenEpoch.clear();
        for (auto iterator = m_items.cbegin(); iterator != m_items.cend(); ++iterator) {
            m_itemLastSeenEpoch.insert(iterator.key(), epoch);
        }
        m_reconcilingPopulation = false;
    } else {
        // The first watcher population is admitted directly because there is
        // no accepted snapshot to preserve; reconcile any unseen keys here.
        for (auto iterator = m_items.begin(); iterator != m_items.end();) {
            const OwnerKey itemKey = iterator.key();
            if (m_itemLastSeenEpoch.value(itemKey, 0) < epoch) {
                if (m_identityOwners.value(iterator->identity) == itemKey) {
                    m_identityOwners.remove(iterator->identity);
                }
                m_itemLastSeenEpoch.remove(itemKey);
                iterator = m_items.erase(iterator);
            } else {
                ++iterator;
            }
        }
    }

    m_hasCompletedPopulation = true;
    m_initialPopulationComplete = true;
    return RegistryOutcome{RegistryStatus::Accepted, {}};
}

quint64 StatusNotifierRegistry::beginOwnerGeneration(quint64 epoch, const QString &uniqueName)
{
    if (epoch == 0 || epoch != m_currentWatcherEpoch) {
        return 0;
    }
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
        dropStagedOwnerItems(uniqueName);
    }
    m_generations.insert(uniqueName, next);
    return next;
}

RegistryOutcome StatusNotifierRegistry::ownerLost(quint64 epoch,
                                                  const QString &uniqueName,
                                                  quint64 expectedGeneration)
{
    if (epoch == 0 || epoch != m_currentWatcherEpoch) {
        return reject(RegistryStatus::StaleOwner, QStringLiteral("stale-watcher-epoch"));
    }
    if (!isValidUniqueBusName(uniqueName)) {
        return reject(RegistryStatus::InvalidOwner, QStringLiteral("owner-not-unique-bus-name"));
    }
    if (!isOwnerLive(uniqueName) || expectedGeneration != currentGeneration(uniqueName)) {
        // A loss that names an unknown owner, a departed owner, or an old
        // generation must not remove a later generation's items.
        return reject(RegistryStatus::StaleOwner, QStringLiteral("generation-not-current"));
    }

    dropOwnerItems(uniqueName);
    dropStagedOwnerItems(uniqueName);
    // The tracking slot is freed immediately: the globally unique generation
    // already fences any late event, so no departed-name residue is needed.
    m_generations.remove(uniqueName);
    return RegistryOutcome{RegistryStatus::Accepted, {}};
}

RegistryOutcome StatusNotifierRegistry::registerItem(quint64 epoch,
                                                     const OwnerKey &key,
                                                     const ItemDescriptor &descriptor)
{
    if (epoch == 0 || epoch != m_currentWatcherEpoch) {
        return reject(RegistryStatus::StaleOwner, QStringLiteral("stale-watcher-epoch"));
    }
    const ValidationOutcome ownerOutcome = validateOwnerKey(key);
    if (!ownerOutcome.accepted) {
        return reject(RegistryStatus::InvalidOwner, ownerOutcome.reasonCode);
    }
    if (!isLiveGeneration(key)) {
        return reject(RegistryStatus::StaleOwner, QStringLiteral("generation-not-current"));
    }

    if (m_reconcilingPopulation) {
        return stageItem(key, descriptor);
    }

    const bool replacesExistingItem = m_items.contains(key);
    const ValidationOutcome descriptorOutcome = validateItemDescriptor(descriptor);
    if (!descriptorOutcome.accepted) {
        // A malformed replacement of a live item degrades the tray but keeps
        // the last-known-good descriptor presented.
        if (replacesExistingItem) {
            // AGENT-GUARD: Membership observation and descriptor admission are
            // separate facts. A malformed current-epoch replacement proves
            // the exact key is still present, so reconciliation must retain
            // its last-known-good descriptor rather than erase it at complete.
            m_itemLastSeenEpoch.insert(key, epoch);
            m_degradedReason = QStringLiteral("malformed-item-replacement");
        }
        return reject(RegistryStatus::InvalidDescriptor, descriptorOutcome.reasonCode);
    }

    const auto identityOwner = m_identityOwners.constFind(descriptor.identity);
    if (identityOwner != m_identityOwners.cend() && identityOwner.value() != key) {
        if (replacesExistingItem) {
            m_itemLastSeenEpoch.insert(key, epoch);
        }
        return reject(RegistryStatus::DuplicateIdentity, QStringLiteral("identity-claimed"));
    }

    if (!replacesExistingItem && m_items.size() >= kMaxItems) {
        m_degradedReason = QStringLiteral("item-capacity-exceeded");
        return reject(RegistryStatus::CapacityExceeded, QStringLiteral("item-capacity-exceeded"));
    }

    const auto existing = m_items.constFind(key);
    if (existing != m_items.cend() && existing->identity != descriptor.identity) {
        m_identityOwners.remove(existing->identity);
    }
    m_items.insert(key, descriptor);
    m_itemLastSeenEpoch.insert(key, epoch);
    m_identityOwners.insert(descriptor.identity, key);
    return RegistryOutcome{RegistryStatus::Accepted, {}};
}

RegistryOutcome StatusNotifierRegistry::removeItem(quint64 epoch, const OwnerKey &key)
{
    if (epoch == 0 || epoch != m_currentWatcherEpoch) {
        return reject(RegistryStatus::StaleOwner, QStringLiteral("stale-watcher-epoch"));
    }
    const ValidationOutcome ownerOutcome = validateOwnerKey(key);
    if (!ownerOutcome.accepted) {
        return reject(RegistryStatus::InvalidOwner, ownerOutcome.reasonCode);
    }
    if (!isLiveGeneration(key)) {
        return reject(RegistryStatus::StaleOwner, QStringLiteral("generation-not-current"));
    }
    if (!m_items.contains(key) && !m_stagedItems.contains(key)) {
        return reject(RegistryStatus::UnknownItem, QStringLiteral("item-not-registered"));
    }
    forgetItem(key);
    forgetStagedItem(key);
    return RegistryOutcome{RegistryStatus::Accepted, {}};
}

RegistryOutcome StatusNotifierRegistry::removeAllForOwner(quint64 epoch,
                                                          const QString &uniqueName,
                                                          quint64 generation)
{
    if (epoch == 0 || epoch != m_currentWatcherEpoch) {
        return reject(RegistryStatus::StaleOwner, QStringLiteral("stale-watcher-epoch"));
    }
    if (!isValidUniqueBusName(uniqueName)) {
        return reject(RegistryStatus::InvalidOwner, QStringLiteral("owner-not-unique-bus-name"));
    }
    if (generation == 0 || !isOwnerLive(uniqueName)
        || generation != currentGeneration(uniqueName)) {
        return reject(RegistryStatus::StaleOwner, QStringLiteral("generation-not-current"));
    }

    dropOwnerItems(uniqueName);
    dropStagedOwnerItems(uniqueName);
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

RegistryOutcome StatusNotifierRegistry::revalidateIntent(const RequestIntent &intent) const
{
    switch (intent.kind) {
    case RequestKind::Activate:
    case RequestKind::ContextMenu:
    case RequestKind::SecondaryActivate:
        break;
    default:
        return reject(RegistryStatus::InvalidRequest, QStringLiteral("unknown-request-kind"));
    }

    const ValidationOutcome ownerOutcome = validateOwnerKey(intent.target);
    if (!ownerOutcome.accepted) {
        return reject(RegistryStatus::InvalidOwner, ownerOutcome.reasonCode);
    }
    if (!isLiveGeneration(intent.target)) {
        return reject(RegistryStatus::StaleOwner, QStringLiteral("generation-not-current"));
    }

    const auto item = m_items.constFind(intent.target);
    if (item == m_items.cend()) {
        return reject(RegistryStatus::UnknownItem, QStringLiteral("item-not-registered"));
    }

    // AGENT-GUARD: If the live key was replaced with a different identity, the
    // in-flight intent is stale and must not be dispatched to the replacement item.
    if (item->identity != intent.identity) {
        return reject(RegistryStatus::InvalidRequest, QStringLiteral("identity-mismatch"));
    }

    return RegistryOutcome{RegistryStatus::Accepted, {}};
}

quint64 StatusNotifierRegistry::currentWatcherEpoch() const noexcept
{
    return m_currentWatcherEpoch;
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

RegistryOutcome StatusNotifierRegistry::stageItem(const OwnerKey &key,
                                                  const ItemDescriptor &descriptor)
{
    const bool replacesPublished = m_items.contains(key);
    const bool replacesStaged = m_stagedItems.contains(key);
    const ValidationOutcome descriptorOutcome = validateItemDescriptor(descriptor);
    if (!descriptorOutcome.accepted) {
        if (replacesStaged || (replacesPublished && stageLastKnownGood(key))) {
            m_degradedReason = QStringLiteral("malformed-item-replacement");
        }
        return reject(RegistryStatus::InvalidDescriptor, descriptorOutcome.reasonCode);
    }

    const auto identityOwner = m_stagedIdentityOwners.constFind(descriptor.identity);
    if (identityOwner != m_stagedIdentityOwners.cend() && identityOwner.value() != key) {
        // An exact LKG key may still be observed with a policy-rejected update;
        // retain its previous descriptor when it does not conflict with the
        // staged target. Otherwise the target contains contradictory identity
        // claims and must not partially replace the published snapshot.
        if (!replacesStaged && (!replacesPublished || !stageLastKnownGood(key))) {
            invalidateStagedPopulation(RegistryStatus::DuplicateIdentity,
                                       QStringLiteral("replacement-population-duplicate-identity"));
        }
        return reject(RegistryStatus::DuplicateIdentity, QStringLiteral("identity-claimed"));
    }

    if (!replacesStaged && m_stagedItems.size() >= kMaxItems) {
        m_degradedReason = QStringLiteral("item-capacity-exceeded");
        invalidateStagedPopulation(RegistryStatus::CapacityExceeded,
                                   QStringLiteral("replacement-population-capacity-exceeded"));
        return reject(RegistryStatus::CapacityExceeded,
                      QStringLiteral("item-capacity-exceeded"));
    }

    const auto existing = m_stagedItems.constFind(key);
    if (existing != m_stagedItems.cend() && existing->identity != descriptor.identity) {
        m_stagedIdentityOwners.remove(existing->identity);
    }
    m_stagedItems.insert(key, descriptor);
    m_stagedIdentityOwners.insert(descriptor.identity, key);
    return RegistryOutcome{RegistryStatus::Accepted, {}};
}

bool StatusNotifierRegistry::stageLastKnownGood(const OwnerKey &key)
{
    if (m_stagedItems.contains(key)) {
        return true;
    }
    const auto published = m_items.constFind(key);
    if (published == m_items.cend()) {
        return false;
    }
    if (m_stagedItems.size() >= kMaxItems) {
        invalidateStagedPopulation(RegistryStatus::CapacityExceeded,
                                   QStringLiteral("replacement-population-cannot-retain-item"));
        return false;
    }
    const auto identityOwner = m_stagedIdentityOwners.constFind(published->identity);
    if (identityOwner != m_stagedIdentityOwners.cend() && identityOwner.value() != key) {
        invalidateStagedPopulation(RegistryStatus::DuplicateIdentity,
                                   QStringLiteral("replacement-population-duplicate-identity"));
        return false;
    }
    m_stagedItems.insert(key, published.value());
    m_stagedIdentityOwners.insert(published->identity, key);
    return true;
}

void StatusNotifierRegistry::invalidateStagedPopulation(RegistryStatus status,
                                                        QString reasonCode)
{
    if (m_stagedPopulationFailure.accepted()) {
        m_stagedPopulationFailure = RegistryOutcome{status, std::move(reasonCode)};
    }
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
    m_itemLastSeenEpoch.remove(key);
    m_items.erase(iterator);
}

void StatusNotifierRegistry::forgetStagedItem(const OwnerKey &key)
{
    const auto iterator = m_stagedItems.constFind(key);
    if (iterator == m_stagedItems.cend()) {
        return;
    }
    if (m_stagedIdentityOwners.value(iterator->identity) == key) {
        m_stagedIdentityOwners.remove(iterator->identity);
    }
    m_stagedItems.erase(iterator);
}

void StatusNotifierRegistry::dropOwnerItems(const QString &uniqueName)
{
    for (auto iterator = m_items.begin(); iterator != m_items.end();) {
        if (iterator.key().uniqueName == uniqueName) {
            if (m_identityOwners.value(iterator->identity) == iterator.key()) {
                m_identityOwners.remove(iterator->identity);
            }
            m_itemLastSeenEpoch.remove(iterator.key());
            iterator = m_items.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

void StatusNotifierRegistry::dropStagedOwnerItems(const QString &uniqueName)
{
    for (auto iterator = m_stagedItems.begin(); iterator != m_stagedItems.end();) {
        if (iterator.key().uniqueName == uniqueName) {
            if (m_stagedIdentityOwners.value(iterator->identity) == iterator.key()) {
                m_stagedIdentityOwners.remove(iterator->identity);
            }
            iterator = m_stagedItems.erase(iterator);
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
