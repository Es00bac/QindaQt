// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/status_notifier/status_notifier_registry.h>

namespace QindaQt::StatusNotifier
{

// AGENT-CONTRACT: The injected transport seam. A StatusNotifierTransport
// implementation owns how tray events arrive (today: only test fakes; a later
// milestone adds an exact-owner QtDBus adapter in its own module). The
// transport drives the registry's generation/keyed-event API and must report
// ownerLost() for every departing unique name; without that report the
// registry cannot fence stale replies after a restart. This interface must
// never grow a real bus connection, name ownership, or service call.
class StatusNotifierTransport
{
public:
    virtual ~StatusNotifierTransport() = default;

    StatusNotifierTransport() = default;
    StatusNotifierTransport(const StatusNotifierTransport &) = delete;
    StatusNotifierTransport &operator=(const StatusNotifierTransport &) = delete;
    StatusNotifierTransport(StatusNotifierTransport &&) = delete;
    StatusNotifierTransport &operator=(StatusNotifierTransport &&) = delete;

    virtual void attach(StatusNotifierRegistry *sink) = 0;
    virtual void detach() = 0;
    [[nodiscard]] virtual bool isAttached() const = 0;
};

} // namespace QindaQt::StatusNotifier
