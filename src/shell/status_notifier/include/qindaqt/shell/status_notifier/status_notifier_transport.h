// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/status_notifier/status_notifier_event_sink.h>

namespace QindaQt::StatusNotifier
{

// AGENT-CONTRACT: The injected transport seam. A StatusNotifierTransport
// implementation owns how tray events arrive (today: only test fakes; a later
// milestone adds an exact-owner QtDBus adapter in its own module). Authority
// is deliberately narrow: a transport sees only StatusNotifierEventSink, so
// it can emit owner/item/epoch events but can never observe items, evaluate
// requests, or acknowledge degradation. This interface must never grow a real
// bus connection, name ownership, or service call.
//
// Lifetime, threading, and attachment rules every implementation must honor:
// - `sink` must be non-owned and must outlive the attachment; call detach()
//   before destroying the sink.
// - attach(nullptr) is refused and leaves the transport unattached.
// - Re-attaching while attached is refused; detach() first. detach() is
//   idempotent and safe when never attached.
// - All sink calls stay on the single thread that performed attach(); the
//   sink is deliberately not synchronized.
// - An implementation destructor must detach itself first.
class StatusNotifierTransport
{
public:
    virtual ~StatusNotifierTransport() = default;

    StatusNotifierTransport() = default;
    StatusNotifierTransport(const StatusNotifierTransport &) = delete;
    StatusNotifierTransport &operator=(const StatusNotifierTransport &) = delete;
    StatusNotifierTransport(StatusNotifierTransport &&) = delete;
    StatusNotifierTransport &operator=(StatusNotifierTransport &&) = delete;

    virtual void attach(StatusNotifierEventSink *sink) = 0;
    virtual void detach() = 0;
    [[nodiscard]] virtual bool isAttached() const = 0;
};

} // namespace QindaQt::StatusNotifier
