// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/status_notifier/status_notifier_types.h>
#include <qindaqt/shell/status_notifier/status_notifier_validation.h>

#include <QtDBus/QDBusConnection>

#include <QtCore/QObject>

#include <functional>

namespace QindaQt::StatusNotifier
{

// Wire-side details that the ADR-0032 value model deliberately has no slot
// for. The monitor consumes itemIsMenu/menuObjectPath through its private
// exported-menu collaborator and the public shared dbusmenu adapter. Window
// and overlay facts remain observation-only; none enter the registry descriptor.
struct ItemWireDetails {
    quint32 windowId = 0;
    QString overlayIconName;
    bool itemIsMenu = false;
    // Exact-owner DBusMenu exporter path consumed by the monitor.
    QString menuObjectPath;

    friend bool operator==(const ItemWireDetails &, const ItemWireDetails &) = default;
};

enum class ItemDescriptorFetchStatus : quint32 {
    ReplyReceived = 0,
    TransportError = 1,
    TimedOut = 2,
};

// Result of one asynchronous descriptor fetch. `status` distinguishes a
// bounded timeout from an immediate transport error. A generation-fenced
// reply is still dropped without emitting any result. Consumers must treat an
// emitted key as observed during population because the registry counts an
// admitted-or-rejected current-epoch registration as observing that live key.
struct ItemDescriptorFetch {
    ItemDescriptorFetchStatus status = ItemDescriptorFetchStatus::TransportError;
    // Compatibility mirror for existing consumers; true exactly when status
    // is ReplyReceived.
    bool replyReceived = false;
    // The exact key the fetch was issued for, generation included, so
    // consumers can route and fence without keeping their own bookkeeping.
    OwnerKey key;
    ItemDescriptor descriptor;
    ItemWireDetails wire;
    ValidationOutcome validation;
    // The owner generation the fetch was tagged with; consumers fence on it.
    quint64 generation = 0;

    friend bool operator==(const ItemDescriptorFetch &, const ItemDescriptorFetch &) = default;
};

// AGENT-CONTRACT: Asynchronous reader for one org.kde.StatusNotifierItem
// object. It fetches the full property set (Category, Id, Title, Status,
// WindowId, IconName, IconPixmap, OverlayIconName, AttentionIconName,
// AttentionPixmap, AttentionMovieName, ToolTip, ItemIsMenu, Menu), decodes
// hostile input defensively, validates through the foundation admission gate,
// and re-fetches whenever any New* signal arrives.
//
// Bounds and fail-closed behavior: pixmap dimensions, byte counts, aggregate
// counts, and text budgets are enforced by the foundation validators; a decode
// shape error or an out-of-bounds value yields an invalid `validation` while
// the descriptor still carries the decoded content so the registry can degrade
// truthfully. Unknown properties are ignored. Presentation-bearing recognized
// properties with unexpected types fail the descriptor closed; recorded-only
// WindowId, OverlayIconName, ItemIsMenu, and Menu facts are safe-dropped on a
// wrong type. Missing or invalid Menu retires the previous exported binding. A missing
// optional property decodes to its default.
//
// Late-reply fencing: every emitted result is tagged with the owner generation
// captured at construction; the injected `GenerationFence` is consulted before
// a result is emitted and a fenced reply is dropped silently, so a reply that
// races owner loss or a watcher rebaseline can never resurrect a removed item.
//
// Lifetime and threading: the injected connection is not owned and must
// outlive the client. All signal traffic stays on the constructing thread;
// there is no internal synchronization. The intent calls (activate and
// friends) are fire-and-forget and do not validate: callers must evaluate and
// revalidate a RequestIntent through the registry before dispatching.
class StatusNotifierItemClient : public QObject
{
    Q_OBJECT

public:
    using GenerationFence = std::function<bool(quint64 generation)>;

    StatusNotifierItemClient(QDBusConnection connection,
                             const OwnerKey &key,
                             GenerationFence generationFence,
                             int fetchTimeoutMs = 5'000,
                             QObject *parent = nullptr);
    ~StatusNotifierItemClient() override;

    [[nodiscard]] OwnerKey key() const;

    // Starts an asynchronous fetch; concurrent New* notifications coalesce
    // into at most one in-flight fetch. Does nothing while a fetch is in
    // flight or when the generation fence already reports the owner stale.
    void fetchDescriptor();

    void activate(int x, int y);
    void secondaryActivate(int x, int y);
    void contextMenu(int x, int y);
    // Orientation must be exactly "horizontal" or "vertical"; anything else
    // is refused (returns false) and nothing is sent.
    [[nodiscard]] bool scroll(int delta, const QString &orientation);

signals:
    void menuDetailsInvalidated();
    void descriptorFetched(const QindaQt::StatusNotifier::ItemDescriptorFetch &result);

private slots:
    void handleNewTitle();
    void handleNewMenu();
    void handlePropertiesChanged(const QString &interface, const QVariantMap &changed,
                                 const QStringList &invalidated);
    void handleNewIcon();
    void handleNewAttentionIcon();
    void handleNewOverlayIcon();
    void handleNewToolTip();
    void handleNewStatus(const QString &status);
    void handleNewIconThemePath(const QString &path);

private:
    void scheduleRefetch();
    void sendIntent(const QString &member, const QList<QVariant> &arguments);
    void finishFetch(ItemDescriptorFetch result);
    [[nodiscard]] bool fenceOpen() const;

    QDBusConnection m_connection;
    OwnerKey m_key;
    GenerationFence m_generationFence;
    int m_fetchTimeoutMs;
    bool m_fetchInFlight = false;
    bool m_fetchDirty = false;
    quint64 m_fetchSerial = 0;
};

} // namespace QindaQt::StatusNotifier

Q_DECLARE_METATYPE(QindaQt::StatusNotifier::ItemWireDetails)
Q_DECLARE_METATYPE(QindaQt::StatusNotifier::ItemDescriptorFetchStatus)
Q_DECLARE_METATYPE(QindaQt::StatusNotifier::ItemDescriptorFetch)
