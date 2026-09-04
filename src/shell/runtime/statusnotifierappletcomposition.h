// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusConnection>
#include <QStringList>

#include <memory>

namespace QindaQt::AppletHost {
class CapabilityPolicy;
}

namespace QindaQt::Applets {
class ManifestCatalog;
}

namespace QindaQt::StatusNotifier {
class StatusNotifierWatcherService;
}

namespace QindaQt::StatusNotifierApplet {
class StatusNotifierAppletController;
class StatusNotifierMonitorAdapter;
}

namespace QindaQt::Shell {

// Shell-private production composition root for the status-notifier tray
// applet. It owns the S1 StatusNotifierWatcher service and the S2 monitor
// adapter (registry + item monitor + icon renderer) on the injected session
// bus and exposes only the least-authority controller facade to the panel.
// The controller's degradation acknowledgement routes through the composed
// adapter seam; nothing outside this object gains StatusNotifier bus
// authority.
class StatusNotifierAppletComposition final
{
public:
    StatusNotifierAppletComposition(
        const Applets::ManifestCatalog &catalog,
        const AppletHost::CapabilityPolicy &policy,
        const QDBusConnection &sessionBus,
        QStringList iconThemeRoots);
    ~StatusNotifierAppletComposition();

    StatusNotifierAppletComposition(const StatusNotifierAppletComposition &) = delete;
    StatusNotifierAppletComposition &operator=(const StatusNotifierAppletComposition &) = delete;

    [[nodiscard]] StatusNotifierApplet::StatusNotifierAppletController *access() const noexcept;

private:
    // AGENT-CONTRACT: reverse destruction is controller -> adapter -> watcher
    // service. The injected connection is borrowed and must outlive this
    // object; the shell removes panel windows (and therefore every QML
    // consumer of the controller) before resetting this composition.
    std::unique_ptr<StatusNotifier::StatusNotifierWatcherService> m_watcher;
    std::unique_ptr<StatusNotifierApplet::StatusNotifierMonitorAdapter> m_adapter;
    std::unique_ptr<StatusNotifierApplet::StatusNotifierAppletController> m_access;
};

} // namespace QindaQt::Shell
