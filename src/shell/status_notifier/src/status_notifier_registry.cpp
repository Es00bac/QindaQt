// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/status_notifier/status_notifier_registry.h>

#include <qindaqt/shell/status_notifier/status_notifier_limits.h>

namespace QindaQt::StatusNotifier
{
namespace
{

// AGENT-NOTE: There is intentionally no clock, timer, or bus here. Fencing
// relies only on owner generations handed out by this registry, so tests can
// script disconnects, restarts, and stale events deterministically.
bool isLiveGeneration(const StatusNotifierRegistry &registry, const OwnerKey &key)
{
    // The liveness flag matters as much as the generation match: after
    // ownerLost() the registry retains the last allocated generation purely
    // for fencing, so a stale reply stamped with that generation must still
    // be rejected instead of resurrecting removed items.
    return key.generation != 0 && key.generation == registry.currentGeneration(key.uniqueName)
        && registry.isOwnerLive(key.uniqueName);
}

} // namespace

quint32 StatusNotifierRegistry::beginOwnerGeneration(const QString &uniqueName)
{
    if (!isValidUniqueBusName(uniqueName)) {
        return 0;
    }
    const quint32 next = m_generations.value(uniqueName, 0) + 1;
    m_generations.insert(uniqueName, next);
    m_ownerLive.insert(uniqueName, true);
    return next;
}

void StatusNotifierRegistry::ownerLost(const QString &uniqueName)
{
    m_ownerLive.insert(uniqueName, false);
    for (auto iterator = m_items.begin(); iterator != m_items.end();) {
        if (iterator.key().uniqueName == uniqueName) {
            m_identityOwners.remove(iterator.value().identity);
            iterator = m_items.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

RegistryOutcome StatusNotifierRegistry::registerItem(const OwnerKey &key,
                                                     const ItemDescriptor &descriptor)
{
    const ValidationOutcome ownerOutcome = validateOwnerKey(key);
    if (!ownerOutcome.accepted) {
        return reject(RegistryStatus::InvalidOwner, ownerOutcome.reasonCode);
    }
    if (!isLiveGeneration(*this, key)) {
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
    if (!isLiveGeneration(*this, key)) {
        return reject(RegistryStatus::StaleOwner, QStringLiteral("generation-not-current"));
    }
    if (!m_items.contains(key)) {
        return reject(RegistryStatus::UnknownItem, QStringLiteral("item-not-registered"));
    }
    forgetItem(key);
    return RegistryOutcome{RegistryStatus::Accepted, {}};
}

RegistryOutcome StatusNotifierRegistry::removeAllForOwner(const QString &uniqueName,
                                                          quint32 generation)
{
    if (!isValidUniqueBusName(uniqueName)) {
        return reject(RegistryStatus::InvalidOwner, QStringLiteral("owner-not-unique-bus-name"));
    }
    if (generation == 0 || generation != currentGeneration(uniqueName)
        || !isOwnerLive(uniqueName)) {
        return reject(RegistryStatus::StaleOwner, QStringLiteral("generation-not-current"));
    }

    for (auto iterator = m_items.begin(); iterator != m_items.end();) {
        if (iterator.key().uniqueName == uniqueName) {
            m_identityOwners.remove(iterator.value().identity);
            iterator = m_items.erase(iterator);
        } else {
            ++iterator;
        }
    }
    return RegistryOutcome{RegistryStatus::Accepted, {}};
}

RegistryOutcome StatusNotifierRegistry::evaluateRequest(const OwnerKey &target,
                                                        RequestKind kind) const
{
    switch (kind) {
    case RequestKind::Activate:
    case RequestKind::ContextMenu:
    case RequestKind::SecondaryActivate:
        break;
    default:
        return reject(RegistryStatus::InvalidRequest, QStringLiteral("unknown-request-kind"));
    }

    const ValidationOutcome ownerOutcome = validateOwnerKey(target);
    if (!ownerOutcome.accepted) {
        return reject(RegistryStatus::InvalidOwner, ownerOutcome.reasonCode);
    }
    if (!isLiveGeneration(*this, target)) {
        return reject(RegistryStatus::StaleOwner, QStringLiteral("generation-not-current"));
    }
    if (!m_items.contains(target)) {
        return reject(RegistryStatus::UnknownItem, QStringLiteral("item-not-registered"));
    }
    return RegistryOutcome{RegistryStatus::Accepted, {}};
}

void StatusNotifierRegistry::markInitialPopulationComplete()
{
    m_initialPopulationComplete = true;
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

quint32 StatusNotifierRegistry::currentGeneration(const QString &uniqueName) const
{
    return m_generations.value(uniqueName, 0);
}

bool StatusNotifierRegistry::isOwnerLive(const QString &uniqueName) const
{
    return m_ownerLive.value(uniqueName, false);
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

} // namespace QindaQt::StatusNotifier
