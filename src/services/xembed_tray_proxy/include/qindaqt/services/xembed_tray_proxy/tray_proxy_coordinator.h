// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QObject>

#include <unordered_map>

#include <QtDBus/QDBusConnection>

#include <functional>
#include <memory>

#include <qindaqt/services/xembed_tray_proxy/tray_backend.h>

class QTimer;

namespace QindaQt::XEmbedTray
{

class StatusNotifierItem;

// Orchestrates the proxy: backend dock events become StatusNotifierItem
// publications, watcher restarts re-register live items, and client lifetime
// retires them. This class names no X11 type (window identity is quint32
// XIDs) and performs no blocking D-Bus calls; both transports live behind
// the injected seams. See ADR-0229.
class TrayProxyCoordinator : public QObject
{
    Q_OBJECT
public:
    // Opens a named connection to the bus the proxy serves. Production uses
    // the session bus; tests use a private bus address.
    using ConnectionFactory = std::function<QDBusConnection(const QString &name)>;

    // AGENT-CONTRACT: docked-icon ceiling matches the shell registry's
    // kMaxItems (status_notifier_limits.h). Exhaustion refuses the dock and
    // logs; the X client keeps its embedded window but gains no SNI item.
    static constexpr qsizetype kMaxDockedIcons = 64;

    TrayProxyCoordinator(XEmbedTrayBackend *backend,
                         ConnectionFactory connectionFactory,
                         QObject *parent = nullptr);
    ~TrayProxyCoordinator() override;

    // False when the X display is unusable (backend start failure). An
    // already-owned selection is not a failure: the coordinator runs inert
    // and claims the selection when the current owner releases it.
    bool start();
    void stop();

    [[nodiscard]] qsizetype itemCount() const;
    [[nodiscard]] bool hasItem(quint32 clientWindow) const;
    [[nodiscard]] QString itemServiceName(quint32 clientWindow) const;
    [[nodiscard]] bool ownsSelection() const;

Q_SIGNALS:
    void itemPublished(quint32 clientWindow);
    void itemRetired(quint32 clientWindow);

private:
    void onDockRequested(quint32 clientWindow);
    void retireItem(quint32 clientWindow);
    void retireAllItems();
    void recapture(quint32 clientWindow);
    void registerItemWithWatcher(const QString &serviceName);
    void registerAllItems();
    void onSelectionLost();

    struct Entry {
        quint32 windowId = 0;
        std::unique_ptr<TrayIconHost> host;
        StatusNotifierItem *item = nullptr;
        QString connectionName;
        QString serviceName;
        QTimer *recaptureTimer = nullptr;
    };

    XEmbedTrayBackend *m_backend = nullptr;
    ConnectionFactory m_connectionFactory;
    QDBusConnection m_anchorConnection{QStringLiteral("invalid")};
    // std::unordered_map because Qt 6.11 QHash::erase() copies nodes
    // on rehash, which a move-only Entry cannot do; node addresses
    // are stable here, which the per-entry lambdas rely on.
    std::unordered_map<quint32, std::unique_ptr<Entry>> m_entries;
    quint32 m_nameCounter = 0;
    bool m_started = false;
};

} // namespace QindaQt::XEmbedTray
