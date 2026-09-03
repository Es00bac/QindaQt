// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/clipboard_model/clipboard_types.h>

#include <QtCore/QtGlobal>

namespace QindaQt::Services::ClipboardWayland {

inline constexpr qsizetype kMaxAdvertisedMediaTypesPerOffer = 64;
inline constexpr qsizetype kMaxPendingOffers = 16;

enum class SelectionKind {
    Clipboard,
    Primary,
};

enum class StartStatus {
    Started,
    AlreadyStarted,
    ConnectionUnavailable,
    UnsupportedPlatform,
};

class CaptureObserver {
public:
    virtual ~CaptureObserver() = default;
    virtual void captureAvailabilityChanged(bool available) = 0;
    virtual void captured(SelectionKind kind,
                          const ClipboardModel::ClipboardValue &value) = 0;
    virtual void captureRefused(SelectionKind kind,
                                ClipboardModel::ClipboardError error) = 0;
};

// Borrowed observer and all calls are confined to the constructing Qt thread.
// stop() destroys every offer/source before disconnecting. Payload bytes are
// held only in memory and are never included in diagnostics.
class ClipboardWaylandAdapter {
public:
    virtual ~ClipboardWaylandAdapter() = default;
    virtual void setObserver(CaptureObserver *observer) = 0;
    [[nodiscard]] virtual StartStatus start() = 0;
    virtual void stop() = 0;
    virtual void setCaptureEnabled(bool enabled) = 0;
    [[nodiscard]] virtual bool isAvailable() const noexcept = 0;
    [[nodiscard]] virtual qint64 peerProcessId() const noexcept = 0;
    [[nodiscard]] virtual bool publishSelection(
        SelectionKind kind, const ClipboardModel::ClipboardValue &value) = 0;
};

} // namespace QindaQt::Services::ClipboardWayland
