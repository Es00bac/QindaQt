// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/xembed_tray_proxy/tray_proxy_coordinator.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QLoggingCategory>
#include <QtCore/QTimer>

#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusPendingCall>
#include <QtDBus/QDBusServiceWatcher>

#include "sni/sni_image_types.h"
#include "sni/status_notifier_item.h"

Q_LOGGING_CATEGORY(lcXEmbedTray, "qindaqt.xembedtray")

namespace QindaQt::XEmbedTray
{

namespace {

constexpr char kWatcherService[] = "org.kde.StatusNotifierWatcher";
constexpr char kWatcherPath[] = "/StatusNotifierWatcher";
constexpr char kWatcherInterface[] = "org.kde.StatusNotifierWatcher";
constexpr char kItemObjectPath[] = "/StatusNotifierItem";
// Damage-driven recapture is coalesced per icon so a client repainting in a
// tight loop produces at most ~30 pixmap publications per second.
constexpr int kRecaptureCoalesceMs = 33;

} // namespace

TrayProxyCoordinator::TrayProxyCoordinator(XEmbedTrayBackend *backend,
                                           ConnectionFactory connectionFactory,
                                           QObject *parent)
    : QObject(parent)
    , m_backend(backend)
    , m_connectionFactory(std::move(connectionFactory))
{
}

TrayProxyCoordinator::~TrayProxyCoordinator()
{
    if (m_started) {
        stop();
    }
}

bool TrayProxyCoordinator::start()
{
    if (m_started) {
        return true;
    }
    registerSniImageTypes();

    m_anchorConnection = m_connectionFactory(
        QStringLiteral("qindaqt-xembed-tray-anchor"));
    if (!m_anchorConnection.isConnected()) {
        qCCritical(lcXEmbedTray, "cannot reach the D-Bus session bus");
        return false;
    }

    if (!m_backend->start()) {
        qCCritical(lcXEmbedTray, "cannot connect to the X display");
        return false;
    }
    m_started = true;

    connect(m_backend, &XEmbedTrayBackend::dockRequested, this,
            &TrayProxyCoordinator::onDockRequested);
    connect(m_backend, &XEmbedTrayBackend::selectionLost, this,
            &TrayProxyCoordinator::onSelectionLost);
    connect(m_backend, &XEmbedTrayBackend::selectionFreed, m_backend,
            &XEmbedTrayBackend::claimSelection);

    if (!m_backend->ownsSelection()) {
        qCInfo(lcXEmbedTray,
               "the XEmbed tray selection is owned elsewhere; proxy is inert "
               "until the owner releases it");
    }

    // AGENT-NOTE: the watcher may legitimately appear after us (session start
    // ordering). Follow its registration and re-register every live item
    // against each fresh watcher instance.
    auto *watcher = new QDBusServiceWatcher(QString::fromLatin1(kWatcherService),
                                            m_anchorConnection,
                                            QDBusServiceWatcher::WatchForRegistration,
                                            this);
    connect(watcher, &QDBusServiceWatcher::serviceRegistered, this,
            &TrayProxyCoordinator::registerAllItems);
    // Cover a watcher that already owns the name at startup.
    QDBusPendingCall ownerCall = m_anchorConnection.interface()->asyncCall(
        QStringLiteral("GetNameOwner"), QString::fromLatin1(kWatcherService));
    auto *ownerWatcher = new QDBusPendingCallWatcher(ownerCall, this);
    connect(ownerWatcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *call) {
                call->deleteLater();
                const QDBusPendingReply<QString> reply = *call;
                if (reply.isValid() && !reply.value().isEmpty()) {
                    registerAllItems();
                }
            });

    return true;
}

void TrayProxyCoordinator::stop()
{
    if (!m_started) {
        return;
    }
    m_started = false;
    retireAllItems();
    m_backend->stop();
}

qsizetype TrayProxyCoordinator::itemCount() const
{
    return qsizetype(m_entries.size());
}

bool TrayProxyCoordinator::hasItem(quint32 clientWindow) const
{
    return m_entries.find(clientWindow) != m_entries.end();
}

QString TrayProxyCoordinator::itemServiceName(quint32 clientWindow) const
{
    const auto it = m_entries.find(clientWindow);
    return it == m_entries.end() ? QString() : it->second->serviceName;
}

bool TrayProxyCoordinator::ownsSelection() const
{
    return m_started && m_backend->ownsSelection();
}

void TrayProxyCoordinator::onDockRequested(quint32 clientWindow)
{
    // AGENT-GUARD: a dock while we do not own the selection would publish an
    // item whose clicks and pixels no other tray owns. The production
    // backend never emits this (client messages only reach the owner); the
    // guard keeps the invariant if a backend misbehaves.
    if (!m_backend->ownsSelection()) {
        qCWarning(lcXEmbedTray,
                  "ignoring dock of window 0x%x: tray selection not owned",
                  clientWindow);
        return;
    }
    if (m_entries.find(clientWindow) != m_entries.end()) {
        // A client re-docking with the same window is a refresh, not an error.
        retireItem(clientWindow);
    } else if (qsizetype(m_entries.size()) >= kMaxDockedIcons) {
        qCWarning(lcXEmbedTray,
                  "refusing to dock window 0x%x: %lld icons already proxied",
                  clientWindow, static_cast<long long>(m_entries.size()));
        return;
    }

    std::unique_ptr<TrayIconHost> host(m_backend->createIconHost(clientWindow));
    if (!host) {
        qCWarning(lcXEmbedTray, "dock of window 0x%x failed: window unusable",
                  clientWindow);
        return;
    }
    if (!host->embed()) {
        qCWarning(lcXEmbedTray, "dock of window 0x%x failed: embed failed",
                  clientWindow);
        return;
    }

    auto entry = std::make_unique<Entry>();
    entry->windowId = clientWindow;
    entry->connectionName =
        QStringLiteral("qindaqt-xembed-item-%1").arg(clientWindow);
    entry->serviceName = entry->connectionName; // replaced below

    QDBusConnection connection = m_connectionFactory(entry->connectionName);
    if (!connection.isConnected()) {
        qCWarning(lcXEmbedTray,
                  "dock of window 0x%x failed: no per-item bus connection",
                  clientWindow);
        return;
    }

    auto *item = new StatusNotifierItem(clientWindow, this);
    item->setTitle(host->clientTitle());
    if (!connection.registerObject(QString::fromLatin1(kItemObjectPath), item,
                                   QDBusConnection::ExportAllProperties
                                       | QDBusConnection::ExportAllSlots
                                       | QDBusConnection::ExportAllSignals)) {
        qCWarning(lcXEmbedTray, "dock of window 0x%x failed: object export failed",
                  clientWindow);
        delete item;
        return;
    }

    // AGENT-NOTE: register the connection's unique name with the watcher. The
    // watcher keys items by owning unique name, and retiring an item is the
    // connection closing — the only removal path the protocol guarantees.
    // The conventional org.kde.StatusNotifierItem-<pid>-<n> name is
    // additionally owned for specification cosmetics and foreign hosts.
    entry->serviceName = connection.baseService();
    const QString conventionalName =
        QStringLiteral("org.kde.StatusNotifierItem-%1-%2")
            .arg(QCoreApplication::applicationPid())
            .arg(m_nameCounter++);
    connection.registerService(conventionalName);

    entry->host = std::move(host);
    entry->item = item;
    TrayIconHost *hostPtr = entry->host.get();

    auto *recaptureTimer = new QTimer(this);
    recaptureTimer->setSingleShot(true);
    recaptureTimer->setInterval(kRecaptureCoalesceMs);
    entry->recaptureTimer = recaptureTimer;

    connect(recaptureTimer, &QTimer::timeout, this,
            [this, clientWindow]() { recapture(clientWindow); });
    connect(hostPtr, &TrayIconHost::iconDamaged, this,
            [this, entry = entry.get()]() {
                if (!entry->recaptureTimer->isActive()) {
                    entry->recaptureTimer->start();
                }
            });
    connect(hostPtr, &TrayIconHost::clientGone, this,
            [this, clientWindow]() { retireItem(clientWindow); });
    connect(hostPtr, &TrayIconHost::clientTitleChanged, item,
            [item, hostPtr]() { item->setTitle(hostPtr->clientTitle()); });
    item->setButtonHandler(
        [hostPtr](quint8 button, qint32 x, qint32 y) {
            hostPtr->forwardButton(button, x, y);
        });

    const QString serviceName = entry->serviceName;
    m_entries.emplace(clientWindow, std::move(entry));

    recapture(clientWindow);
    registerItemWithWatcher(serviceName);
    Q_EMIT itemPublished(clientWindow);
}

void TrayProxyCoordinator::recapture(quint32 clientWindow)
{
    const auto it = m_entries.find(clientWindow);
    if (it == m_entries.end()) {
        return;
    }
    const TrayIconImage image = it->second->host->captureIcon();
    if (!image.isNull()) {
        it->second->item->setIcon(image);
    }
}

void TrayProxyCoordinator::retireItem(quint32 clientWindow)
{
    const auto it = m_entries.find(clientWindow);
    if (it == m_entries.end()) {
        return;
    }
    Entry *entry = it->second.get();
    // AGENT-GUARD: close the per-item connection before deleting the item,
    // so the watcher observes owner loss for a still-registered name. The
    // host is retired first so a dying client cannot race a half-torn-down
    // item.
    entry->host->retire();
    const QString connectionName = entry->connectionName;
    delete entry->item;
    entry->item = nullptr;
    m_entries.erase(it);
    QDBusConnection::disconnectFromBus(connectionName);
    Q_EMIT itemRetired(clientWindow);
}

void TrayProxyCoordinator::retireAllItems()
{
    QList<quint32> windows;
    windows.reserve(qsizetype(m_entries.size()));
    for (const auto &pair : m_entries) {
        windows.push_back(pair.first);
    }
    for (quint32 window : windows) {
        retireItem(window);
    }
}

void TrayProxyCoordinator::onSelectionLost()
{
    qCInfo(lcXEmbedTray,
           "lost the XEmbed tray selection; retiring %lld proxied item(s)",
           static_cast<long long>(m_entries.size()));
    // AGENT-CONTRACT: retirement reparents clients back to the root window
    // (TrayIconHost::retire) so the new selection owner can dock them.
    retireAllItems();
}

void TrayProxyCoordinator::registerItemWithWatcher(const QString &serviceName)
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(kWatcherService), QString::fromLatin1(kWatcherPath),
        QString::fromLatin1(kWatcherInterface),
        QStringLiteral("RegisterStatusNotifierItem"));
    call.setArguments({serviceName});
    m_anchorConnection.asyncCall(call);
}

void TrayProxyCoordinator::registerAllItems()
{
    for (const auto &pair : m_entries) {
        registerItemWithWatcher(pair.second->serviceName);
    }
}

} // namespace QindaQt::XEmbedTray
