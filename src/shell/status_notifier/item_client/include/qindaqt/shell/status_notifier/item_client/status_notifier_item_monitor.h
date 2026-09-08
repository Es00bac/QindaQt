// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/status_notifier/item_client/status_notifier_item_client.h>
#include <qindaqt/shell/status_notifier/status_notifier_registry.h>
#include <qindaqt/shell/status_notifier/status_notifier_transport.h>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusServiceWatcher>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QVariantMap>

namespace QindaQt::StatusNotifier
{

class StatusNotifierItemMenu;

// AGENT-CONTRACT: The production StatusNotifierTransport. It watches whichever
// connection owns org.kde.StatusNotifierWatcher — QindaQt's own
// StatusNotifierWatcherService or a foreign KDE watcher — and is the only
// feeder of the foundation registry: every owner/item event flows through the
// attached StatusNotifierEventSink with the exact epoch and owner generation
// the registry requires (ADR-0032), so a reply that races a disconnect,
// restart, or watcher transition is rejected as stale instead of resurrecting
// removed items.
//
// Epoch flow: watcher (re)appearance opens a fresh watcher epoch and drives a
// bounded initial population from RegisteredStatusNotifierItems; the
// completion event is emitted only after every population key was observed
// (admitted or rejected) or timed out. StatusNotifierItemRegistered/Unregistered
// drive mid-epoch additions and removals, and a bus-daemon-authenticated
// per-owner NameOwnerChanged loss drives ownerLost with the current
// generation, which also frees the registry's bounded owner slot. Watcher
// disappearance itself does not touch the registry: presentation degrades
// through isWatcherLive() while last-known-good items stay visible and
// actionable.
//
// Request intents (Activate, SecondaryActivate, ContextMenu, Scroll) are
// validated as RequestIntents through the registry — evaluateRequest plus an
// immediate revalidateIntent before dispatch — and only then marshalled onto
// the owning item's D-Bus object. The monitor never executes an intent that
// the registry has not validated for the current generation and identity.
//
// Lifetime and threading: the injected connection and registry are not owned
// and must outlive the monitor. All D-Bus and sink traffic stays on the
// attaching thread; there is no internal synchronization. attach() refuses a
// null sink and re-attachment; detach() is idempotent and the destructor
// detaches first (StatusNotifierTransport contract).
class StatusNotifierItemMonitor final : public QObject, public StatusNotifierTransport
{
    Q_OBJECT

public:
    StatusNotifierItemMonitor(QDBusConnection connection,
                              StatusNotifierRegistry &registry,
                              int fetchTimeoutMs = 5'000,
                              QObject *parent = nullptr);
    ~StatusNotifierItemMonitor() override;

    void attach(StatusNotifierEventSink *sink) override;
    void detach() override;
    [[nodiscard]] bool isAttached() const override;

    // True while a watcher owns the watched name on the injected connection.
    // Shell presentation maps this to PresentationInput.transportLive.
    [[nodiscard]] bool isWatcherLive() const;

    // Intent dispatch: validated through the registry (evaluate + revalidate),
    // then sent to the owning item. A non-accepted outcome means nothing was
    // sent; the returned outcome names the refusal.
    [[nodiscard]] RegistryOutcome requestActivate(const OwnerKey &target, int x, int y);
    [[nodiscard]] RegistryOutcome requestSecondaryActivate(const OwnerKey &target,
                                                           int x,
                                                           int y);
    [[nodiscard]] RegistryOutcome requestContextMenu(const OwnerKey &target, int x, int y);
    [[nodiscard]] RegistryOutcome requestScroll(const OwnerKey &target,
                                                int delta,
                                                const QString &orientation);

    // Exact-generation menu projection, with decimal revision and bounded
    // recursive entries. All wire decoding stays in the shared dbusmenu
    // client. Pending/failed refreshes retain display but block invocation.
    [[nodiscard]] bool itemIsMenu(const OwnerKey &target) const;
    [[nodiscard]] bool hasExportedMenu(const OwnerKey &target) const;
    [[nodiscard]] QVariantMap menuState(const OwnerKey &target) const;
    [[nodiscard]] RegistryOutcome openMenu(const OwnerKey &target, int x, int y);
    [[nodiscard]] RegistryOutcome aboutToShowMenu(const OwnerKey &target, quint64 revision, int id);
    [[nodiscard]] RegistryOutcome invokeMenu(const OwnerKey &target, quint64 revision, int id);

signals:
    void menuChanged();
    void watcherLiveChanged(bool live);

private slots:
    void handleWatcherServiceChange(const QString &serviceName);
    void handleItemRegistered(const QString &serviceId);
    void handleItemUnregistered(const QString &serviceId);
    void handleNameOwnerChanged(const QString &name,
                                const QString &oldOwner,
                                const QString &newOwner);

private:
    struct ItemSlot {
        OwnerKey key;
        StatusNotifierItemClient *client = nullptr;
        StatusNotifierItemMenu *menu = nullptr;
        bool populationPending = false;
    };

    void beginEpochAndPopulate();
    void fetchRegisteredItems();
    void admitPopulationItem(const QString &serviceId);
    void handleDescriptorFetched(const ItemDescriptorFetch &result);
    void completePopulationIfDrained();
    void watchItemOwner(const QString &uniqueName, const QString &objectPath);
    // Validates the intent through the registry and resolves the live watched
    // client for `target`; returns failure without touching the client when
    // the intent is not currently valid.
    [[nodiscard]] RegistryOutcome validateIntentForDispatch(const OwnerKey &target,
                                                            RequestKind kind,
                                                            const ItemSlot **slot) const;
    void resetEpochState();
    [[nodiscard]] quint64 nextMenuRevision();
    void setWatcherLive(bool live);

    [[nodiscard]] static bool parseServiceId(const QString &serviceId,
                                             QString *uniqueName,
                                             QString *objectPath);

    QDBusConnection m_connection;
    StatusNotifierRegistry &m_registry;
    StatusNotifierEventSink *m_sink = nullptr;
    int m_fetchTimeoutMs;
    quint64 m_epoch = 0;
    quint64 m_menuRevision = 0;
    bool m_populationInFlight = false;
    qsizetype m_populationOutstanding = 0;
    bool m_watcherLive = false;
    // AGENT-GUARD: Slots are keyed by "uniqueName/objectPath", never by
    // generation: an owner rebaseline or re-registration must replace the
    // slot deterministically instead of accumulating stale clients.
    QHash<QString, ItemSlot> m_slots;
    // AGENT-GUARD: An item path can retire while its unique owner stays live.
    // Keep that owner's one epoch generation independently from m_slots or a
    // later path can incorrectly rebase every remaining registry fact.
    QHash<QString, quint64> m_ownerGenerations;
};

} // namespace QindaQt::StatusNotifier
