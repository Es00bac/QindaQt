// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/status_notifier/icon/status_notifier_icon_renderer.h>
#include <qindaqt/shell/status_notifier/item_client/status_notifier_item_monitor.h>
#include <qindaqt/shell/status_notifier/status_notifier_registry.h>
#include "qindaqt/shell/status_notifier/applet/status_notifier_source_interface.h"

#include <QtDBus/QDBusConnection>

#include <QtCore/QStringList>

#include <memory>

namespace QindaQt::StatusNotifierApplet {

// AGENT-CONTRACT: Production composition of the applet source seam over the
// S1 StatusNotifier stack. The adapter owns the registry, the interposed
// forwarding sink, the item monitor, and the icon renderer; it serves the
// controller's observation and intent surface from them. The watcher SERVICE
// is owned by the future shell-session composition, not by this adapter. All
// D-Bus wire traffic stays inside the S1 monitor: the adapter never opens
// connections and never calls QDBus APIs beyond storing the injected
// connection type.
//
// Lifetime and threading: the injected connection is not owned and must
// outlive the adapter. Everything is GUI-thread confined — the monitor's
// sink calls, the renderer, and the emitted changed() signals all stay on
// the thread that called start(). stop() is idempotent and the destructor
// stops first, so the monitor never outlives the sink it is attached to.
class StatusNotifierMonitorAdapter final : public StatusNotifierSourceInterface {
    Q_OBJECT

public:
    explicit StatusNotifierMonitorAdapter(QDBusConnection connection,
                                          QStringList iconThemeRoots,
                                          int fetchTimeoutMs = 5'000,
                                          QObject *parent = nullptr);
    ~StatusNotifierMonitorAdapter() override;

    StatusNotifierMonitorAdapter(const StatusNotifierMonitorAdapter &) = delete;
    StatusNotifierMonitorAdapter &operator=(const StatusNotifierMonitorAdapter &) = delete;

    // Attaches the monitor through the forwarding sink. Idempotent.
    void start();
    // Detaches the monitor. Idempotent; safe when never started.
    void stop();
    [[nodiscard]] bool isRunning() const;

    [[nodiscard]] QindaQt::StatusNotifier::TrayPresentation presentation() const override;
    [[nodiscard]] QList<QindaQt::StatusNotifier::ItemDescriptor> itemDescriptors() const override;
    [[nodiscard]] QImage renderIcon(const QindaQt::StatusNotifier::OwnerKey &key,
                                    int size) const override;
    [[nodiscard]] quint64 currentGeneration(const QString &uniqueName) const override;

    [[nodiscard]] QindaQt::StatusNotifier::RegistryOutcome activate(
        const QindaQt::StatusNotifier::OwnerKey &target, int x, int y) override;
    [[nodiscard]] QindaQt::StatusNotifier::RegistryOutcome secondaryActivate(
        const QindaQt::StatusNotifier::OwnerKey &target, int x, int y) override;
    [[nodiscard]] QindaQt::StatusNotifier::RegistryOutcome contextMenu(
        const QindaQt::StatusNotifier::OwnerKey &target, int x, int y) override;

private:
    // AGENT-CONTRACT: forwarding sink interposed between the monitor and the
    // registry. The S1 monitor emits only watcherLiveChanged and has no
    // registry-change signal, so this shim forwards every StatusNotifierEventSink
    // call verbatim — including the beginWatcherEpoch/beginOwnerGeneration
    // return values — to the owned registry and emits the adapter's changed()
    // after each accepted mutation. It adds no policy of its own.
    class ForwardingSink;

    QindaQt::StatusNotifier::StatusNotifierRegistry m_registry;
    std::unique_ptr<ForwardingSink> m_sink;
    QindaQt::StatusNotifier::StatusNotifierItemMonitor m_monitor;
    QindaQt::StatusNotifier::StatusNotifierIconRenderer m_renderer;
    bool m_running = false;
};

} // namespace QindaQt::StatusNotifierApplet
