// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/status_notifier/item_client/status_notifier_item_monitor.h>

#include <qindaqt/shell/status_notifier/status_notifier_limits.h>
#include <qindaqt/shell/status_notifier/status_notifier_validation.h>

#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QPointer>
#include <QTimer>

namespace QindaQt::StatusNotifier
{

StatusNotifierItemMonitor::StatusNotifierItemMonitor(QDBusConnection connection,
                                                     StatusNotifierRegistry &registry,
                                                     int fetchTimeoutMs,
                                                     QObject *parent)
    : QObject(parent)
    , m_connection(std::move(connection))
    , m_registry(registry)
    , m_fetchTimeoutMs(fetchTimeoutMs)
{
    // AGENT-GUARD: Match the bus daemon as sender. Path/interface/member are
    // forgeable by another peer, which must not retire a live registry owner.
    m_connection.connect(QStringLiteral("org.freedesktop.DBus"),
                         QStringLiteral("/org/freedesktop/DBus"),
                         QStringLiteral("org.freedesktop.DBus"),
                         QStringLiteral("NameOwnerChanged"),
                         this,
                         SLOT(handleNameOwnerChanged(QString,QString,QString)));
}

StatusNotifierItemMonitor::~StatusNotifierItemMonitor()
{
    detach();
}

void StatusNotifierItemMonitor::attach(StatusNotifierEventSink *sink)
{
    if (sink == nullptr || m_sink != nullptr) {
        return; // Contract: null-first refusal and no re-attachment.
    }
    m_sink = sink;

    // AGENT-CONTRACT: the watcher signals retire/re-admit individual items
    // (e.g. after its owner-loss sweep); the owner-loss match below is the
    // authoritative per-owner retire path. Both may fire for one loss — the
    // registry refuses the duplicate as stale, so ordering is not a contract.
    m_connection.connect(QString::fromLatin1(kWatcherServiceName),
                         QString::fromLatin1(kWatcherObjectPath),
                         QString::fromLatin1(kWatcherInterfaceName),
                         QStringLiteral("StatusNotifierItemRegistered"),
                         this,
                         SLOT(handleItemRegistered(QString)));
    m_connection.connect(QString::fromLatin1(kWatcherServiceName),
                         QString::fromLatin1(kWatcherObjectPath),
                         QString::fromLatin1(kWatcherInterfaceName),
                         QStringLiteral("StatusNotifierItemUnregistered"),
                         this,
                         SLOT(handleItemUnregistered(QString)));

    auto *serviceWatcher = new QDBusServiceWatcher(
        QString::fromLatin1(kWatcherServiceName),
        m_connection,
        QDBusServiceWatcher::WatchForOwnerChange,
        this);
    connect(serviceWatcher, &QDBusServiceWatcher::serviceRegistered, this,
            &StatusNotifierItemMonitor::handleWatcherServiceChange);
    connect(serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, this,
            &StatusNotifierItemMonitor::handleWatcherServiceChange);

    // A watcher that already owns the name opens the first epoch immediately.
    const QDBusReply<QString> owner =
        m_connection.interface()->serviceOwner(QString::fromLatin1(kWatcherServiceName));
    if (owner.isValid() && !owner.value().isEmpty()) {
        setWatcherLive(true);
        beginEpochAndPopulate();
    }
}

void StatusNotifierItemMonitor::detach()
{
    if (m_sink == nullptr) {
        return; // Idempotent, safe when never attached.
    }
    resetEpochState();
    m_sink = nullptr;
    m_epoch = 0;
    setWatcherLive(false);
    for (QObject *child : findChildren<QDBusServiceWatcher *>()) {
        delete child;
    }
}

bool StatusNotifierItemMonitor::isAttached() const
{
    return m_sink != nullptr;
}

bool StatusNotifierItemMonitor::isWatcherLive() const
{
    return m_watcherLive;
}

RegistryOutcome StatusNotifierItemMonitor::requestActivate(const OwnerKey &target,
                                                           int x,
                                                           int y)
{
    const ItemSlot *slot = nullptr;
    const RegistryOutcome outcome =
        validateIntentForDispatch(target, RequestKind::Activate, &slot);
    if (outcome.accepted()) {
        slot->client->activate(x, y);
    }
    return outcome;
}

RegistryOutcome StatusNotifierItemMonitor::requestSecondaryActivate(const OwnerKey &target,
                                                                    int x,
                                                                    int y)
{
    const ItemSlot *slot = nullptr;
    const RegistryOutcome outcome =
        validateIntentForDispatch(target, RequestKind::SecondaryActivate, &slot);
    if (outcome.accepted()) {
        slot->client->secondaryActivate(x, y);
    }
    return outcome;
}

RegistryOutcome StatusNotifierItemMonitor::requestContextMenu(const OwnerKey &target,
                                                              int x,
                                                              int y)
{
    const ItemSlot *slot = nullptr;
    const RegistryOutcome outcome =
        validateIntentForDispatch(target, RequestKind::ContextMenu, &slot);
    if (outcome.accepted()) {
        slot->client->contextMenu(x, y);
    }
    return outcome;
}

RegistryOutcome StatusNotifierItemMonitor::requestScroll(const OwnerKey &target,
                                                         int delta,
                                                         const QString &orientation)
{
    if (orientation != QStringLiteral("horizontal")
        && orientation != QStringLiteral("vertical")) {
        return {RegistryStatus::InvalidRequest, QStringLiteral("invalid-scroll-orientation")};
    }
    const ItemSlot *slot = nullptr;
    const RegistryOutcome outcome =
        validateIntentForDispatch(target, RequestKind::Scroll, &slot);
    if (outcome.accepted()) {
        const bool sent = slot->client->scroll(delta, orientation);
        Q_UNUSED(sent) // Orientation was validated above; the client re-checks.
    }
    return outcome;
}

void StatusNotifierItemMonitor::handleWatcherServiceChange(const QString &)
{
    const QDBusReply<QString> owner =
        m_connection.interface()->serviceOwner(QString::fromLatin1(kWatcherServiceName));
    const bool live = owner.isValid() && !owner.value().isEmpty();
    if (live == m_watcherLive) {
        return;
    }
    if (live) {
        // A (re)appearing watcher — including a replacement process — begins
        // a fresh epoch; the registry reconciles the staged population at the
        // matching completion event.
        beginEpochAndPopulate();
    }
    setWatcherLive(live);
}

void StatusNotifierItemMonitor::handleItemRegistered(const QString &serviceId)
{
    QString uniqueName;
    QString objectPath;
    if (m_sink == nullptr || !parseServiceId(serviceId, &uniqueName, &objectPath)
        || !isValidUniqueBusName(uniqueName) || !isValidObjectPath(objectPath)) {
        return;
    }
    // The first key observed for an owner in this epoch rebaselines it. Later
    // paths from that same owner share the issued generation.
    watchItemOwner(uniqueName, objectPath);
    const QString slotKey = uniqueName + QLatin1Char('/') + objectPath;
    const auto iterator = m_slots.constFind(slotKey);
    if (iterator != m_slots.cend()) {
        iterator->client->fetchDescriptor();
    }
}

void StatusNotifierItemMonitor::handleItemUnregistered(const QString &serviceId)
{
    QString uniqueName;
    QString objectPath;
    if (m_sink == nullptr || !parseServiceId(serviceId, &uniqueName, &objectPath)) {
        return;
    }
    const QString slotKey = uniqueName + QLatin1Char('/') + objectPath;
    const auto iterator = m_slots.constFind(slotKey);
    if (iterator == m_slots.cend()) {
        return;
    }
    if (iterator->populationPending) {
        m_populationOutstanding--;
    }
    delete iterator->client;
    m_slots.erase(iterator);
    completePopulationIfDrained();

    const quint64 generation = m_registry.currentGeneration(uniqueName);
    if (generation != 0) {
        OwnerKey key;
        key.uniqueName = uniqueName;
        key.objectPath = objectPath;
        key.generation = generation;
        m_sink->removeItem(m_epoch, key);
    }
}

void StatusNotifierItemMonitor::handleNameOwnerChanged(const QString &name,
                                                       const QString &oldOwner,
                                                       const QString &newOwner)
{
    if (m_sink == nullptr || name != oldOwner || !isValidUniqueBusName(name)
        || !newOwner.isEmpty() || !m_registry.isOwnerLive(name)) {
        return;
    }
    // The owner disconnected: retire it with its current generation so the
    // registry drops its items and frees the bounded tracking slot. A
    // watcher's matching StatusNotifierItemUnregistered signals that follow
    // are refused by the registry as stale — harmless by design.
    const quint64 generation = m_registry.currentGeneration(name);
    m_sink->ownerLost(m_epoch, name, generation);
    m_ownerGenerations.remove(name);
    for (auto iterator = m_slots.begin(); iterator != m_slots.end();) {
        if (iterator.key().startsWith(name + QLatin1Char('/'))) {
            if (iterator->populationPending) {
                m_populationOutstanding--;
            }
            delete iterator->client;
            iterator = m_slots.erase(iterator);
        } else {
            ++iterator;
        }
    }
    completePopulationIfDrained();
}

void StatusNotifierItemMonitor::beginEpochAndPopulate()
{
    if (m_sink == nullptr) {
        return;
    }
    resetEpochState();
    const quint64 epoch = m_sink->beginWatcherEpoch();
    if (epoch == 0) {
        // Epoch exhaustion fails closed: no events are admitted until the
        // next watcher transition opens a usable epoch.
        return;
    }
    m_epoch = epoch;
    m_populationInFlight = true;
    m_populationOutstanding = 0;
    fetchRegisteredItems();
}

void StatusNotifierItemMonitor::fetchRegisteredItems()
{
    // AGENT-NOTE: the Get goes out with an EMPTY interface field: the
    // hyphenated Properties name is rejected client-side by asyncCall's
    // validation (and silently dropped by QDBusInterface), while member-name
    // dispatch reaches the watcher's Properties adaptor unchanged. See the
    // matching note in StatusNotifierItemClient::fetchDescriptor.
    auto request = QDBusMessage::createMethodCall(
        QString::fromLatin1(kWatcherServiceName),
        QString::fromLatin1(kWatcherObjectPath),
        QString(),
        QStringLiteral("Get"));
    request << QVariant(QString::fromLatin1(kWatcherInterfaceName))
            << QVariant(QStringLiteral("RegisteredStatusNotifierItems"));
    QPointer<QDBusPendingCallWatcher> watcher =
        new QDBusPendingCallWatcher(m_connection.asyncCall(request), this);
    QTimer::singleShot(m_fetchTimeoutMs, this, [this, watcher]() {
        // QPointer guard: the finished lambda may already have deleteLater()d
        // (and event delivery freed) the watcher before this timer fires.
        if (watcher) {
            watcher->deleteLater();
        }
        if (!m_populationInFlight || m_populationOutstanding != 0) {
            return; // Individual item timeouts still gate completion.
        }
        m_populationInFlight = false;
        m_sink->markInitialPopulationComplete(m_epoch);
    });
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *call) {
                call->deleteLater();
                if (!m_populationInFlight) {
                    return;
                }
                const QDBusMessage reply = call->reply();
                QStringList serviceIds;
                if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
                    QVariant value = reply.arguments().constFirst();
                    if (value.canConvert<QDBusVariant>()) {
                        value = value.value<QDBusVariant>().variant();
                    }
                    if (value.canConvert<QStringList>()) {
                        serviceIds = value.toStringList();
                    }
                }
                // A failed property read degrades to an empty population; the
                // watcher restart path re-populates on the next transition.
                for (const QString &serviceId : std::as_const(serviceIds)) {
                    admitPopulationItem(serviceId);
                }
                completePopulationIfDrained();
            });
}

void StatusNotifierItemMonitor::admitPopulationItem(const QString &serviceId)
{
    QString uniqueName;
    QString objectPath;
    if (!parseServiceId(serviceId, &uniqueName, &objectPath)
        || !isValidUniqueBusName(uniqueName) || !isValidObjectPath(objectPath)) {
        // A malformed watcher entry cannot be keyed; it is not observable and
        // must not block population completion.
        return;
    }
    watchItemOwner(uniqueName, objectPath);
    const auto iterator = m_slots.find(uniqueName + QLatin1Char('/') + objectPath);
    if (iterator == m_slots.end() || iterator->populationPending) {
        return; // Begin refused, malformed entry, or duplicate population key.
    }
    iterator->populationPending = true;
    m_populationOutstanding++;
    iterator->client->fetchDescriptor();
}

void StatusNotifierItemMonitor::handleDescriptorFetched(const ItemDescriptorFetch &result)
{
    if (m_sink == nullptr) {
        return;
    }
    const QString slotKey = result.key.uniqueName + QLatin1Char('/') + result.key.objectPath;
    const auto iterator = m_slots.find(slotKey);
    if (iterator == m_slots.end() || iterator->key.generation != result.generation) {
        // Stale reply from a replaced slot: fence it, but still drain any
        // population accounting it carried so completion cannot wedge.
        if (iterator != m_slots.end() && iterator->populationPending) {
            iterator->populationPending = false;
            m_populationOutstanding--;
            completePopulationIfDrained();
        }
        return;
    }
    // AGENT-CONTRACT: The registration is issued for every observed key, even
    // when the descriptor is invalid or the reply never arrived; the registry
    // counts a rejected current-epoch admission as observing the exact live
    // key and keeps the last-known-good descriptor presented.
    m_sink->registerItem(m_epoch, iterator->key, result.descriptor);
    if (iterator->populationPending) {
        iterator->populationPending = false;
        m_populationOutstanding--;
    }
    completePopulationIfDrained();
}

void StatusNotifierItemMonitor::completePopulationIfDrained()
{
    if (!m_populationInFlight || m_populationOutstanding != 0) {
        return;
    }
    m_populationInFlight = false;
    if (m_sink != nullptr) {
        m_sink->markInitialPopulationComplete(m_epoch);
    }
}

void StatusNotifierItemMonitor::watchItemOwner(const QString &uniqueName,
                                               const QString &objectPath)
{
    const QString slotKey = uniqueName + QLatin1Char('/') + objectPath;
    if (m_slots.contains(slotKey)) {
        return;
    }
    // AGENT-GUARD: Item-path lifetime is shorter than unique-owner lifetime.
    // The owner ledger, not surviving path slots, is the epoch-generation
    // authority; otherwise retiring its last path lets a later path rebase a
    // still-live owner within the same watcher epoch.
    quint64 generation = m_ownerGenerations.value(uniqueName);
    if (generation == 0) {
        generation = m_sink->beginOwnerGeneration(m_epoch, uniqueName);
    }
    if (generation == 0) {
        // Capacity or counter exhaustion refuses the begin; the key cannot be
        // observed and last-known-good truth is left untouched.
        return;
    }
    m_ownerGenerations.insert(uniqueName, generation);
    OwnerKey key;
    key.uniqueName = uniqueName;
    key.objectPath = objectPath;
    key.generation = generation;

    ItemSlot slot;
    slot.key = key;
    slot.populationPending = false;
    slot.client = new StatusNotifierItemClient(
        m_connection, key,
        [this, uniqueName](quint64 generationToCheck) {
            return m_registry.currentGeneration(uniqueName) == generationToCheck;
        },
        m_fetchTimeoutMs, this);
    connect(slot.client, &StatusNotifierItemClient::descriptorFetched, this,
            &StatusNotifierItemMonitor::handleDescriptorFetched);
    m_slots.insert(slotKey, slot);
}

RegistryOutcome StatusNotifierItemMonitor::validateIntentForDispatch(
    const OwnerKey &target, RequestKind kind, const ItemSlot **slot) const
{
    if (m_sink == nullptr) {
        return {RegistryStatus::InvalidRequest, QStringLiteral("monitor-not-attached")};
    }
    const auto evaluation = m_registry.evaluateRequest(target, kind);
    if (!evaluation.outcome.accepted()) {
        return evaluation.outcome;
    }
    const RegistryOutcome check = m_registry.revalidateIntent(evaluation.intent);
    if (!check.accepted()) {
        return check;
    }
    const QString slotKey = target.uniqueName + QLatin1Char('/') + target.objectPath;
    const auto iterator = m_slots.constFind(slotKey);
    if (iterator == m_slots.cend() || iterator->key.generation != target.generation) {
        return {RegistryStatus::UnknownItem, QStringLiteral("item-not-watched")};
    }
    *slot = &iterator.value();
    return {RegistryStatus::Accepted, {}};
}

void StatusNotifierItemMonitor::resetEpochState()
{
    for (auto iterator = m_slots.begin(); iterator != m_slots.end(); ++iterator) {
        delete iterator->client;
    }
    m_slots.clear();
    m_ownerGenerations.clear();
    m_populationOutstanding = 0;
    m_populationInFlight = false;
}

void StatusNotifierItemMonitor::setWatcherLive(bool live)
{
    if (m_watcherLive == live) {
        return;
    }
    m_watcherLive = live;
    emit watcherLiveChanged(live);
}

bool StatusNotifierItemMonitor::parseServiceId(const QString &serviceId,
                                               QString *uniqueName,
                                               QString *objectPath)
{
    const qsizetype separator = serviceId.indexOf(QLatin1Char('/'));
    // AGENT-GUARD: P1-4 regression: when the separator is the final byte, the
    // suffix is the valid root object path "/", not a missing path.
    if (separator <= 0) {
        return false;
    }
    *uniqueName = serviceId.left(separator);
    *objectPath = serviceId.mid(separator);
    return true;
}

} // namespace QindaQt::StatusNotifier
